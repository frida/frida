namespace Frida {
	/**
	 * Searches for and installs npm packages of Frida agents, for use with the
	 * {@link Compiler}.
	 */
	public sealed class PackageManager : Object {
		/**
		 * Emitted as an install progresses.
		 *
		 * @param phase the current phase of the installation
		 * @param fraction progress within the phase, from 0 to 1
		 * @param details optional human-readable detail
		 */
		public signal void install_progress (PackageInstallPhase phase, double fraction, string? details = null);

		/**
		 * The npm registry host to use.
		 */
		public string registry {
			get;
			set;
			default = "registry.npmjs.org";
		}

		private Soup.Session session;

		/**
		 * Creates a package manager.
		 */
		public PackageManager () {
			string cache_dir = Path.build_filename (Environment.get_user_cache_dir (), "frida", "package-manager");
			var cache = new Soup.Cache (cache_dir, Soup.CacheType.SINGLE_USER);
			session = (Soup.Session) Object.new (typeof (Soup.Session),
				"max-conns-per-host", 15);
			session.add_feature (cache);
		}

		/**
		 * Searches the registry for packages matching a query.
		 *
		 * @param query the search query
		 * @param options search options such as paging, or null
		 * @return the matching packages
		 */
		public async PackageSearchResult search (string query, PackageSearchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var opts = (options != null) ? options : new PackageSearchOptions ();

			var text = string.join (" ", query, "keywords:frida-gum");
			Json.Reader reader = yield fetch ("/-/v1/search?text=%s&from=%u&size=%u"
				.printf (Uri.escape_string (text), opts.offset, opts.limit), cancellable);

			var packages = new Gee.ArrayList<Package> ();
			reader.read_member ("objects");
			int count = reader.count_elements ();
			if (count == -1)
				throw new Error.PROTOCOL ("Unexpected JSON format: 'objects' array missing");
			for (int i = 0; i != count; i++) {
				reader.read_element (i);

				reader.read_member ("package");

				reader.read_member ("name");
				string? name = reader.get_string_value ();
				reader.end_member ();

				reader.read_member ("version");
				string? version = reader.get_string_value ();
				reader.end_member ();

				reader.read_member ("description");
				string? description = reader.get_string_value ();
				reader.end_member ();

				reader.read_member ("links");
				reader.read_member ("npm");
				string? url = reader.get_string_value ();
				reader.end_member ();
				reader.end_member ();

				reader.end_member ();

				if (name == null || version == null || url == null)
					throw new Error.PROTOCOL ("Unexpected JSON format: missing package details");
				packages.add (new Package (name, version, description, url));

				reader.end_element ();
			}
			reader.end_member ();

			reader.read_member ("total");
			int64 total_count = reader.get_int_value ();
			if (total_count == -1)
				throw new Error.PROTOCOL ("Unexpected JSON format: 'total' missing");

			return new PackageSearchResult (new PackageList (packages), (uint) total_count);
		}

		public PackageSearchResult search_sync (string query, PackageSearchOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<SearchTask> ();
			task.query = query;
			task.options = options;
			return task.execute (cancellable);
		}

		private class SearchTask : PackageManagerTask<PackageSearchResult> {
			public string query;
			public PackageSearchOptions? options;

			protected override async PackageSearchResult perform_operation () throws Error, IOError {
				return yield parent.search (query, options, cancellable);
			}
		}

		/**
		 * Installs packages into a project, emitting {@link
		 * PackageManager.install_progress} as it goes.
		 *
		 * @param options install options, such as the project directory and the
		 *   packages to install, or null
		 * @return the result of the installation
		 */
		public async PackageInstallResult install (PackageInstallOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			install_progress (INITIALIZING, 0.0);

			var opts = (options != null) ? options : new PackageInstallOptions ();

			File project_root = compute_project_root (opts);
			File pkg_json_file = project_root.get_child ("package.json");
			File lock_file = project_root.get_child ("package-lock.json");

			var cache = new PackageDataCache (this, cancellable);

			var specs = parse_mutation_specs (ADD, opts.specs, opts.role);
			var manifest = yield read_manifest (pkg_json_file, cancellable);
			bool dirty = yield apply_specs_to_manifest (specs, manifest, cache, cancellable);

			install_progress (PREPARING_DEPENDENCIES, 0.05);

			Gee.Map<string, PackageLockPackageInfo>? locked = yield read_lockfile (lock_file, cancellable);

			bool lock_clean = !dirty && locked != null && deps_match (manifest, locked[""].dependencies);

			var installed_packages = new Gee.ArrayList<Package> ();

			if (lock_clean) {
				var root = lock_to_graph (manifest, locked);
				yield reify_graph (root, project_root, opts.omits, installed_packages, 0.05, 0.98, cancellable);
			} else {
				PackageNode? base_graph = null;
				if (locked != null)
					base_graph = lock_to_graph (manifest, locked);

				var tree = yield build_tree (manifest, base_graph, cache, cancellable);

				foreach (var orphan in tree.orphans)
					yield FS.rmtree_async (install_dir_for_dependency (orphan, project_root), cancellable);

				yield reify_graph (tree.root, project_root, opts.omits, installed_packages, 0.85, 0.98, cancellable);

				yield write_manifest (manifest, pkg_json_file, cancellable);
				yield write_lockfile (manifest, tree.root, lock_file, cancellable);
			}

			install_progress (COMPLETE, 1.0);

			installed_packages.sort ((a, b) => strcmp (a.name, b.name));

			return new PackageInstallResult (new PackageList (installed_packages));
		}

		private async BuildTreeResult build_tree (Manifest manifest, PackageNode? base_node, PackageDataCache cache,
				Cancellable? cancellable) throws Error, IOError {
			if (base_node == null) {
				var root = yield build_tree_from_scratch (manifest, cache, cancellable);
				return new BuildTreeResult () {
					root = root,
					orphans = new Gee.ArrayList<string> (),
				};
			}

			var to_unlock = collect_packages_needing_unlock (manifest, base_node);
			var anchors = selective_yank_nodes (to_unlock);
			if (!anchors.contains (base_node))
				anchors.add (base_node);

			yield patch_holes (anchors, manifest, cache, 0.05, 0.75, cancellable);

			var orphaned = remove_orphaned_packages (base_node);

			install_progress (RESOLVING_PACKAGE, 0.80);
			if (hoist_graph (base_node))
				trim_orphaned_now_in_graph (base_node, orphaned);
			install_progress (RESOLVING_PACKAGE, 0.85);

			return new BuildTreeResult () {
				root = base_node,
				orphans = orphaned.paths,
			};
		}

		private class BuildTreeResult {
			public PackageNode root;
			public Gee.Collection<string> orphans;
		}

		private async PackageNode build_tree_from_scratch (Manifest manifest, PackageDataCache cache, Cancellable? cancellable)
				throws Error, IOError {
			var root = yield build_ideal_graph (manifest, cache, 0.05, 0.75, cancellable);

			install_progress (RESOLVING_PACKAGE, 0.80);
			hoist_graph (root);
			install_progress (RESOLVING_PACKAGE, 0.85);

			return root;
		}

		private async PackageNode build_ideal_graph (Manifest manifest, PackageDataCache cache, double start_fraction,
				double end_fraction, Cancellable? cancellable) throws Error, IOError {
			var root = new PackageNode (manifest.name, manifest.version, manifest.dependencies);

			var q = new Gee.ArrayQueue<DepQueueItem> ();
			foreach (PackageDependency d in root.active_deps.values) {
				if (d.role == PEER)
					continue;
				var future = cache.fetch (d.name, d.version);
				q.offer (new DepQueueItem (root, d, future));
			}

			var expanded = new Gee.HashSet<string> ();
			uint total_top_level_deps = q.size;
			uint resolved_top_level_deps = 0;
			double fraction_span = end_fraction - start_fraction;

			DepQueueItem? item;
			while ((item = q.poll ()) != null) {
				var host = item.host;
				var dep = item.dep;
				var pdata = yield item.data_future.wait_async (cancellable);

				if (host == root) {
					resolved_top_level_deps++;
					double current_fraction =
						start_fraction + (fraction_span * resolved_top_level_deps / total_top_level_deps);
					install_progress (RESOLVING_PACKAGE, current_fraction);
				}

				var node = add_child_node (host, dep, pdata);

				if (expanded.add (pdata.resolved_url)) {
					foreach (PackageDependency cd in node.active_deps.values) {
						if (cd.role == PEER)
							continue;
						var future = cache.fetch (cd.name, cd.version);
						q.offer (new DepQueueItem (node, cd, future));
					}
				}
			}

			return root;
		}

		private static async PackageNode build_ideal_subtree (string name, string spec, PackageDataCache cache,
				Cancellable? cancellable) throws Error, IOError {
			ResolvedPackageData pdata = yield cache.fetch (name, new PackageVersion (spec)).wait_async (cancellable);

			var root = new PackageNode (pdata.name, pdata.effective_version, pdata.dependencies);
			root.license = pdata.license;
			root.funding = pdata.funding;
			root.engines = pdata.engines;
			root.os = pdata.os;
			root.cpu = pdata.cpu;
			root.libc = pdata.libc;
			root.resolved = pdata.resolved_url;
			root.integrity = pdata.integrity;

			var q = new Gee.ArrayQueue<DepQueueItem> ();
			foreach (PackageDependency d in root.active_deps.values) {
				if (d.role == DEVELOPMENT || d.role == PEER)
					continue;
				var future = cache.fetch (d.name, d.version);
				q.offer (new DepQueueItem (root, d, future));
			}

			var expanded = new Gee.HashSet<string> ();

			DepQueueItem? item;
			while ((item = q.poll ()) != null) {
				var host = item.host;
				var dep = item.dep;
				var ddata = yield item.data_future.wait_async (cancellable);

				var node = add_child_node (host, dep, ddata);

				if (expanded.add (ddata.resolved_url)) {
					foreach (PackageDependency cd in node.active_deps.values) {
						if (cd.role == PEER)
							continue;
						var future = cache.fetch (cd.name, cd.version);
						q.offer (new DepQueueItem (node, cd, future));
					}
				}
			}

			return root;
		}

		private static PackageNode add_child_node (PackageNode host, PackageDependency dep, ResolvedPackageData data)
				throws Error {
			var n = new PackageNode (data.name, data.effective_version, data.dependencies);
			n.optional_peers = data.optional_peers;

			n.is_optional = (dep.role == OPTIONAL) || data.optional_peers.contains (dep.name);
			n.deprecated = data.deprecated;
			n.license = data.license;
			n.funding = data.funding;
			n.engines = data.engines;
			n.os = data.os;
			n.cpu = data.cpu;
			n.libc = data.libc;

			n.resolved = data.resolved_url;
			n.integrity = data.integrity;

			host.children[data.name] = n;
			n.parent = host;
			return n;
		}

		private static Gee.Set<PackageNode> collect_packages_needing_unlock (Manifest manifest, PackageNode base_node)
				throws Error {
			var to_unlock = new Gee.HashSet<PackageNode> ();

			foreach (PackageDependency dep in manifest.dependencies.all.values) {
				var existing_node = base_node.find_provider (dep.name);
				if (existing_node == null)
					continue;

				if (!Semver.satisfies_range (existing_node.version, dep.version.range)) {
					to_unlock.add (existing_node);
					collect_dependent_packages (existing_node, base_node, dep.version.range, to_unlock);
				}
			}

			return to_unlock;
		}

		private static void collect_dependent_packages (PackageNode target, PackageNode root, string new_version_range,
				Gee.Set<PackageNode> to_unlock) throws Error {
			SemverVersion? new_version = null;
			try {
				new_version = Semver.parse_version (new_version_range);
			} catch (Error e) {
			}

			var dependents = find_direct_dependents (target, root);

			foreach (var dependent in dependents) {
				if (to_unlock.contains (dependent))
					continue;

				var dep_requirement = dependent.active_deps[target.name];
				if (dep_requirement == null)
					continue;

				bool would_be_satisfied = (new_version != null)
					? Semver.satisfies_range (new_version, dep_requirement.version.range)
					: false;
				if (!would_be_satisfied) {
					to_unlock.add (dependent);
					collect_dependent_packages (dependent, root, dep_requirement.version.range, to_unlock);
				}
			}
		}

		private static Gee.List<PackageNode> find_direct_dependents (PackageNode target, PackageNode root) {
			var dependents = new Gee.ArrayList<PackageNode> ();
			find_direct_dependents_recursive (target.name, root, dependents);
			return dependents;
		}

		private static void find_direct_dependents_recursive (string target_name, PackageNode current,
				Gee.List<PackageNode> result) {
			if (current.active_deps.has_key (target_name))
				result.add (current);

			foreach (var child in current.children.values)
				find_direct_dependents_recursive (target_name, child, result);
		}

		private static Gee.List<PackageNode> selective_yank_nodes (Gee.Set<PackageNode> to_unlock) {
			var anchors = new Gee.ArrayList<PackageNode> ();

			foreach (var node in to_unlock) {
				if (node.parent != null) {
					var parent = node.parent;
					parent.children.unset (node.name);
					node.parent = null;

					if (!to_unlock.contains (parent))
						anchors.add (parent);
				}
			}

			return anchors;
		}

		private async void patch_holes (Gee.List<PackageNode> anchors, Manifest manifest, PackageDataCache cache,
				double start_fraction, double end_fraction, Cancellable? cancellable) throws Error, IOError {
			var chunks = new Gee.ArrayList<HolePatchChunk> ();
			uint total_to_patch = 0;
			foreach (PackageNode anchor in anchors) {
				Gee.Map<string, string> needed = (anchor.parent == null)
					? anchor.find_missing_ranges (manifest)
					: find_missing_dependencies_for_subtree (anchor);
				chunks.add (new HolePatchChunk () {
					anchor = anchor,
					needed = needed,
				});
				total_to_patch += needed.size;
			}

			if (total_to_patch == 0) {
				install_progress (RESOLVING_PACKAGE, end_fraction);
				return;
			}

			uint completed = 0;
			double fraction_span = end_fraction - start_fraction;

			foreach (HolePatchChunk chunk in chunks) {
				PackageNode anchor = chunk.anchor;

				foreach (var kv in chunk.needed.entries) {
					var sub = yield build_ideal_subtree (kv.key, kv.value, cache, cancellable);
					anchor.children[kv.key] = sub;
					sub.parent = anchor;

					completed++;
					double current_fraction = start_fraction + (fraction_span * completed / total_to_patch);
					install_progress (RESOLVING_PACKAGE, current_fraction);
				}
			}
		}

		private class HolePatchChunk {
			public PackageNode anchor;
			public Gee.Map<string, string> needed;
		}

		private static Gee.Map<string, string> find_missing_dependencies_for_subtree (PackageNode anchor) {
			var missing = new Gee.HashMap<string, string> ();
			var processed = new Gee.HashSet<PackageNode> ();

			collect_missing_from_subtree (anchor, missing, processed);

			return missing;
		}

		private static void collect_missing_from_subtree (PackageNode node, Gee.Map<string, string> missing,
				Gee.Set<PackageNode> processed) {
			if (!processed.add (node))
				return;

			foreach (var dep in node.active_deps.values) {
				if (node.find_provider (dep.name) == null) {
					if (!missing.has_key (dep.name))
						missing[dep.name] = dep.version.range;
				}
			}

			foreach (var child in node.children.values)
				collect_missing_from_subtree (child, missing, processed);
		}

		private static Orphaned remove_orphaned_packages (PackageNode root) {
			mark_reachable_packages (root);

			var result = new Orphaned () {
				packages = new Gee.HashSet<string> (),
				paths = new Gee.HashSet<string> (),
			};
			sweep_unreachable_packages (root, result.packages, result.paths);

			reset_marks (root);

			return result;
		}

		private class Orphaned {
			public Gee.Set<string> packages;
			public Gee.Set<string> paths;
		}

		private static void trim_orphaned_now_in_graph (PackageNode root, Orphaned orphaned) {
			foreach (string dir_path in orphaned.paths.to_array ()) {
				PackageNode? current_package = find_package_at_path (root, dir_path);
				if (current_package != null) {
					string current_id = "%s@%s".printf (current_package.name, current_package.version.str);
					if (orphaned.packages.remove (current_id))
						orphaned.paths.remove (dir_path);
				}
			}
		}

		private static PackageNode? find_package_at_path (PackageNode root, string target_path) {
			if (root.compute_path () == target_path)
				return root;

			foreach (var child in root.children.values) {
				var result = find_package_at_path (child, target_path);
				if (result != null)
					return result;
			}

			return null;
		}

		private static void mark_reachable_packages (PackageNode root) {
			var visited = new Gee.HashSet<PackageNode> ();
			var queue = new Gee.LinkedList<PackageNode> ();

			root.marked = true;
			queue.offer (root);

			PackageNode? node;
			while ((node = queue.poll ()) != null) {
				if (!visited.add (node))
					continue;

				foreach (var dep in node.active_deps.values) {
					var provider = node.find_provider (dep.name);
					if (provider != null && !provider.marked) {
						provider.marked = true;
						queue.offer (provider);
					}
				}
			}
		}

		private static void sweep_unreachable_packages (PackageNode node, Gee.Set<string> orphaned_packages,
				Gee.Set<string> orphaned_paths) {
			foreach (var child in node.children.values.to_array ()) {
				if (!child.marked) {
					orphaned_packages.add ("%s@%s".printf (child.name, child.version.str));
					orphaned_paths.add (child.compute_path ());

					node.children.unset (child.name);
					child.parent = null;
				} else {
					sweep_unreachable_packages (child, orphaned_packages, orphaned_paths);
				}
			}
		}

		private static void reset_marks (PackageNode node) {
			node.marked = false;

			foreach (var child in node.children.values)
				reset_marks (child);
		}

		private class DepQueueItem {
			public PackageNode host;
			public PackageDependency dep;
			public Future<ResolvedPackageData> data_future;

			public DepQueueItem (PackageNode host, PackageDependency dep, Future<ResolvedPackageData> data_future) {
				this.host = host;
				this.dep = dep;
				this.data_future = data_future;
			}
		}

		private static bool hoist_graph (PackageNode root) throws Error {
			bool any_hoisted = false;
			while (try_hoist_any_package (root))
				any_hoisted = true;
			return any_hoisted;
		}

		private static bool try_hoist_any_package (PackageNode root) throws Error {
			bool any_hoisted = false;

			var bfs = new Gee.ArrayQueue<PackageNode> ();
			var seen = new Gee.HashSet<PackageNode> ();

			bfs.offer (root);
			seen.add (root);

			PackageNode? n;
			while ((n = bfs.poll ()) != null) {
				foreach (var child in n.children.values.to_array ()) {
					if (try_hoist (child))
						any_hoisted = true;

					if (child.parent != null && seen.add (child))
						bfs.offer (child);
				}
			}

			return any_hoisted;
		}

		private static bool try_hoist (PackageNode node) throws Error {
			while (true) {
				var parent = node.parent;
				if (parent == null || parent.parent == null)
					return false;

				for (var anc = parent.parent; anc != null; anc = anc.parent) {
					var dupe = anc.children[node.name];
					if (dupe == null)
						continue;

					if (dupe.version.str == node.version.str) {
						if (!satisfies_all_ancestor_siblings (node, anc))
							return false;

						merge_children (dupe, node);
						parent.children.unset (node.name);
						node.parent = null;
						return true;
					}

					bool dupe_breaks_req = false;
					foreach (var sib in anc.children.values) {
						if (sib == parent)
							continue;

						var edge = sib.active_deps[node.name];
						if (edge != null &&
								!Semver.satisfies_range (dupe.version, edge.version.range) &&
								Semver.satisfies_range (node.version, edge.version.range)) {
							dupe_breaks_req = true;
							break;
						}
					}
					if (!dupe_breaks_req)
						return false;

					if (!satisfies_all_ancestors (node, anc))
						return false;

					if (!satisfies_all_ancestor_siblings (node, anc))
						return false;

					parent.children[node.name] = dupe;
					dupe.parent = parent;

					anc.children[node.name] = node;
					node.parent = anc;

					return true;
				}

				var anc = parent.parent;

				if (!satisfies_ancestor (node, anc))
					return false;

				if (!ancestor_satisfies_dependencies (anc, node.dependencies.peer.values))
					return false;

				parent.children.unset (node.name);

				anc.children[node.name] = node;
				node.parent = anc;

				return true;
			}
		}

		private static bool satisfies_ancestor (PackageNode node, PackageNode anc) throws Error {
			var need = anc.active_deps[node.name];
			if (need == null)
				return true;

			return Semver.satisfies_range (node.version, need.version.range);
		}

		private static bool satisfies_all_ancestors (PackageNode node, PackageNode anc_or_higher) throws Error {
			for (var a = anc_or_higher; a != null; a = a.parent) {
				if (!satisfies_ancestor (node, a))
					return false;
			}
			return true;
		}

		private static bool satisfies_all_ancestor_siblings (PackageNode node, PackageNode anc_or_higher) throws Error {
			for (var a = anc_or_higher; a != null; a = a.parent) {
				foreach (var sib in a.children.values) {
					if (sib == node)
						continue;

					var edge = sib.active_deps[node.name];
					if (edge == null)
						continue;

					PackageNode? child = sib.children[node.name];
					SemverVersion v = (child != null) ? child.version : node.version;

					if (!Semver.satisfies_range (v, edge.version.range))
						return false;
				}
			}
			return true;
		}

		private static bool ancestor_satisfies_dependencies (PackageNode anc, Gee.Collection<PackageDependency> deps) throws Error {
			foreach (PackageDependency d in deps) {
				var provider = anc.find_provider (d.name);
				if (provider == null || !Semver.satisfies_range (provider.version, d.version.range))
					return false;
			}
			return true;
		}

		private static void merge_children (PackageNode target, PackageNode donor) {
			foreach (var gc in donor.children.values) {
				if (!target.children.has_key (gc.name)) {
					target.children[gc.name] = gc;
					gc.parent = target;
				}
			}
		}

		private static Gee.List<MutationSpec> parse_mutation_specs (MutationKind kind, Gee.List<string> raw_specs,
				PackageRole role) {
			var specs = new Gee.ArrayList<MutationSpec> ();
			foreach (string raw_spec in raw_specs) {
				int sep_offset = raw_spec.has_prefix ("@") ? 1 : 0;
				int sep = raw_spec.index_of_char ('@', sep_offset);

				string name = (sep != -1) ? raw_spec[:sep] : raw_spec;
				string range = (sep != -1) ? raw_spec[sep + 1:] : "";

				specs.add (new MutationSpec () {
					kind = kind,
					name = name,
					range = range,
					role = role,
				});
			}
			return specs;
		}

		private static async bool apply_specs_to_manifest (Gee.List<MutationSpec> specs, Manifest manifest, PackageDataCache cache,
				Cancellable? cancellable) throws Error, IOError {
			bool dirty = false;
			PackageDependencies deps = manifest.dependencies;

			foreach (var s in specs) {
				switch (s.kind) {
					case ADD:
						string new_range;
						if (s.range == "" || s.range == "latest") {
							var resolved_data = yield cache.fetch (s.name, new PackageVersion ("latest"))
								.wait_async (cancellable);
							new_range = "^" + resolved_data.effective_version.str;
						} else {
							new_range = s.range;
						}

						PackageDependency? dep = deps.all[s.name];
						if (dep == null || new_range != dep.version.range || s.role != dep.role) {
							deps.remove (s.name);
							deps.add (new PackageDependency () {
								name = s.name,
								version = new PackageVersion (new_range),
								role = s.role,
							});
							dirty = true;
						}

						break;
					case REMOVE:
						dirty |= deps.all.unset (s.name);
						break;
				}
			}

			return dirty;
		}

		private static async Manifest read_manifest (File location, Cancellable? cancellable) throws Error, IOError {
			var m = new Manifest ();

			if (location.query_exists (cancellable)) {
				var pkg_json = yield FS.read_all_text (location, cancellable);
				detect_indent (pkg_json, out m.indent_level, out m.indent_char);

				Json.Reader r = parse_json (new Bytes.static (pkg_json.data));

				r.read_member ("name");
				m.name = r.get_string_value ();
				r.end_member ();

				r.read_member ("version");
				string? raw_version = r.get_string_value ();
				m.version = (raw_version != null) ? Semver.parse_version (raw_version) : null;
				r.end_member ();

				m.license = read_license (r);
				m.funding = read_funding (r);
				m.engines = read_engines (r);
				m.dependencies = read_dependencies (r);

				m.json = pkg_json;
				m.root = r.root;
			} else {
				m.root = new Json.Node (OBJECT);
				m.root.init_object (new Json.Object ());
			}

			return m;
		}

		private static async void write_manifest (Manifest manifest, File location, Cancellable? cancellable)
				throws Error, IOError {
			Json.Node root = manifest.root;
			Json.Object obj = root.get_object ();

			set_manifest_deps (obj, "dependencies", manifest.dependencies.runtime.values);
			set_manifest_deps (obj, "devDependencies", manifest.dependencies.development.values);
			set_manifest_deps (obj, "optionalDependencies", manifest.dependencies.optional.values);
			set_manifest_deps (obj, "peerDependencies", manifest.dependencies.peer.values);

			string json = generate_npm_style_json (root, manifest.indent_level, manifest.indent_char);
			bool pkg_json_changed = manifest.json == null || json != manifest.json;
			if (pkg_json_changed)
				yield FS.write_all_text (location, json, cancellable);
		}

		private static bool deps_match (Manifest manifest, PackageDependencies deps) {
			foreach (var manifest_dep in manifest.dependencies.all.values) {
				var lock_dep = deps.all[manifest_dep.name];
				if (lock_dep == null)
					return false;

				if (lock_dep.version.range != manifest_dep.version.range)
					return false;

				if (lock_dep.role != manifest_dep.role)
					return false;
			}

			foreach (var lock_dep in deps.all.values) {
				var manifest_dep = manifest.dependencies.all[lock_dep.name];
				if (manifest_dep == null)
					return false;
			}

			return true;
		}

		private static void set_manifest_deps (Json.Object root, string section_name,
				Gee.Collection<PackageDependency> dependencies) {
			if (dependencies.is_empty) {
				root.remove_member (section_name);
				return;
			}

			var deps = new Json.Object ();
			foreach (PackageDependency dep in dependencies)
				deps.set_string_member (dep.name, dep.version.range);
			root.set_object_member (section_name, deps);
		}

		private static async Gee.Map<string, PackageLockPackageInfo>? read_lockfile (File lock_file, Cancellable? cancellable)
				throws Error, IOError {
			if (!lock_file.query_exists (cancellable))
				return null;

			var packages = new Gee.HashMap<string, PackageLockPackageInfo> ();

			Json.Reader lock_r = yield load_json (lock_file, cancellable);

			lock_r.read_member ("packages");
			string[]? path_keys = lock_r.list_members ();
			if (path_keys == null)
				throw new Error.PROTOCOL ("Lockfile 'packages' member missing or not an object");

			foreach (unowned string path_key in path_keys) {
				bool is_root_package = path_key == "";

				lock_r.read_member (path_key);

				if (lock_r.get_null_value ()) {
					lock_r.end_member ();
					continue;
				}

				var pli = new PackageLockPackageInfo ();

				if (is_root_package) {
					lock_r.read_member ("name");
					pli.name = lock_r.get_string_value ();
					lock_r.end_member ();
				} else {
					int last_start = path_key.last_index_of ("/node_modules/");
					pli.name = (last_start != -1)
						? path_key[last_start + 14:]
						: path_key[13:];
				}

				lock_r.read_member ("version");
				string? raw_version = lock_r.get_string_value ();
				if (!is_root_package && raw_version == null)
					throw new Error.PROTOCOL ("Lockfile 'version' for '%s' missing or invalid", path_key);
				pli.version = (raw_version != null) ? Semver.parse_version (raw_version) : null;
				lock_r.end_member ();

				pli.dependencies = read_dependencies (lock_r);

				lock_r.read_member ("dev");
				pli.is_dev = lock_r.get_boolean_value ();
				lock_r.end_member ();

				lock_r.read_member ("optional");
				pli.is_optional = lock_r.get_boolean_value ();
				lock_r.end_member ();

				pli.deprecated = read_deprecated (lock_r);
				pli.optional_peers = read_optional_peers (lock_r);
				pli.bin = read_bin (lock_r);
				pli.has_install_script = read_has_install_script (lock_r);
				pli.license = read_license (lock_r);
				pli.funding = read_funding (lock_r);
				pli.engines = read_engines (lock_r);
				pli.os = read_array_str (lock_r, "os");
				pli.cpu = read_array_str (lock_r, "cpu");
				pli.libc = read_array_str (lock_r, "libc");

				if (!is_root_package) {
					lock_r.read_member ("resolved");
					pli.resolved = lock_r.get_string_value ();
					if (pli.resolved == null) {
						throw new Error.PROTOCOL ("Lockfile 'resolved' for '%s' missing or invalid",
							path_key);
					}
					lock_r.end_member ();

					lock_r.read_member ("integrity");
					pli.integrity = lock_r.get_string_value ();
					if (pli.integrity == null) {
						throw new Error.PROTOCOL ("Lockfile 'integrity' for '%s' missing or invalid",
							path_key);
					}
					lock_r.end_member ();
				}

				packages[path_key] = pli;

				lock_r.end_member ();
			}

			lock_r.end_member ();

			return packages;
		}

		private async void write_lockfile (Manifest manifest, PackageNode root, File lock_file, Cancellable? cancellable)
				throws Error, IOError {
			var b = new Json.Builder ();
			b.begin_object ();
			string? name = manifest.name;
			if (name == null) {
				try {
					var info = yield lock_file.get_parent ().query_info_async (FileAttribute.STANDARD_DISPLAY_NAME,
						FileQueryInfoFlags.NONE, Priority.DEFAULT, cancellable);
					name = info.get_display_name ();
				} catch (GLib.Error e) {
					throw new Error.PERMISSION_DENIED ("%s", e.message);
				}
			}
			write_name (name, b);
			write_version (manifest.version, b);
			b.set_member_name ("lockfileVersion").add_int_value (3);
			b.set_member_name ("requires").add_boolean_value (true);
			b.set_member_name ("packages").begin_object ();

			b.set_member_name ("").begin_object ();
			write_name (manifest.name, b);
			write_version (manifest.version, b);
			write_license (manifest.license, b);
			write_funding (manifest.funding, b);
			write_dependencies_section ("dependencies", manifest.dependencies.runtime, b);
			write_dependencies_section ("devDependencies", manifest.dependencies.development, b);
			write_engines (manifest.engines, b);
			b.end_object ();

			var path_map = build_path_map (root);
			var dev_only_packages = compute_dev_only_packages (root);
			var optional_only_packages = compute_optional_only_packages (root);

			foreach (var e in path_map.entries) {
				string k = e.key;
				if (k == "")
					continue;
				PackageNode pn = e.value;

				b.set_member_name (k).begin_object ();
				write_version (pn.version, b);
				b
					.set_member_name ("resolved")
					.add_string_value (pn.resolved)
					.set_member_name ("integrity")
					.add_string_value (pn.integrity);

				write_array_str ("cpu", pn.cpu, b);
				write_has_install_script (pn.has_install_script, b);
				write_license (pn.license, b);

				if (dev_only_packages.contains (k))
					b.set_member_name ("dev").add_boolean_value (true);
				else if (optional_only_packages.contains (k))
					b.set_member_name ("optional").add_boolean_value (true);

				write_array_str ("os", pn.os, b);
				write_array_str ("libc", pn.libc, b);

				write_deprecated (pn.deprecated, b);
				write_bin (pn.bin, b);
				write_dependencies_section ("dependencies", pn.dependencies.runtime, b);
				write_engines (pn.engines, b);
				write_dependencies_section ("optionalDependencies", pn.dependencies.optional, b);
				write_optional_peers (pn.optional_peers, b);
				write_funding (pn.funding, b);
				write_dependencies_section ("peerDependencies", pn.dependencies.peer, b);

				b.end_object ();
			}

			b
				.end_object ()
				.end_object ();

			string txt = generate_npm_style_json (b.get_root (), manifest.indent_level, manifest.indent_char);
			yield FS.write_all_text (lock_file, txt, cancellable);
		}

		private static PackageNode lock_to_graph (Manifest manifest, Gee.Map<string, PackageLockPackageInfo> packages)
				throws Error {
			var root = new PackageNode (manifest.name, manifest.version, manifest.dependencies);

			foreach (var e in packages.entries) {
				string key = e.key;
				PackageLockPackageInfo pkg = e.value;

				if (key == "")
					continue;

				string[] segs = key.split ("/");
				PackageNode cur = root;
				for (int i = 0; i != segs.length; ) {
					if (segs[i] != "node_modules")
						throw new Error.PROTOCOL ("Invalid lockfile");
					i++;
					if (i >= segs.length)
						throw new Error.PROTOCOL ("Invalid lockfile");

					string name;
					if (segs[i].has_prefix ("@")) {
						if (i + 1 == segs.length)
							throw new Error.PROTOCOL ("Invalid lockfile");
						name = segs[i] + "/" + segs[i + 1];
						i += 2;
					} else {
						name = segs[i];
						i += 1;
					}

					PackageNode? next = cur.children[name];
					if (next == null) {
						next = new PackageNode (name);
						cur.children[name] = next;
						next.parent = cur;
					}
					cur = next;
				}

				cur.version = pkg.version;
				cur.dependencies = pkg.dependencies;
				cur.optional_peers = pkg.optional_peers;

				cur.is_optional = pkg.is_optional;
				cur.deprecated = pkg.deprecated;
				cur.bin = pkg.bin;
				cur.has_install_script = pkg.has_install_script;
				cur.license = pkg.license;
				cur.funding = pkg.funding;
				cur.engines = pkg.engines;
				cur.os = pkg.os;
				cur.cpu = pkg.cpu;
				cur.libc = pkg.libc;

				cur.resolved = pkg.resolved;
				cur.integrity = pkg.integrity;
			}

			return root;
		}

		private async void reify_graph (PackageNode root, File project_root, Gee.Set<PackageRole> omits,
				Gee.List<Package> installed_packages, double start_fraction, double end_fraction, Cancellable? cancellable)
				throws Error, IOError {
			var inner = new Cancellable ();

			var source = new CancellableSource (cancellable);
			source.set_callback (() => {
				inner.cancel ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());

			var path_map = build_path_map (root);
			var packages_to_install = compute_packages_to_install (path_map, omits);
			var futures = new Gee.ArrayList<Future<bool>> ();
			uint completed = 0;
			double fraction_span = end_fraction - start_fraction;
			Error? first_error = null;

			perform_reify_graph (root, project_root, packages_to_install, null, futures, installed_packages, inner);

			if (futures.is_empty)
				return;

			foreach (var f in futures) {
				f.then (fut => {
					try {
						fut.get_result ();
					} catch (GLib.Error e) {
						if (!(e is IOError.CANCELLED) && first_error == null)
							first_error = (Error) e;
						inner.cancel ();
					}

					completed++;

					double current_fraction = start_fraction + (fraction_span * completed / futures.size);
					install_progress (RESOLVING_AND_INSTALLING_ALL, current_fraction);

					if (completed == futures.size)
						reify_graph.callback ();
				});
			}
			yield;
			if (first_error != null)
				throw first_error;
		}

		private static Gee.Set<string> compute_packages_to_install (Gee.Map<string, PackageNode> path_map,
				Gee.Set<PackageRole> omits) throws Error {
			var to_install = new Gee.HashSet<string> ();
			collect_all_package_paths (path_map[""], to_install);

			if (omits.contains (DEVELOPMENT)) {
				var dev_only_packages = compute_dev_only_packages (path_map[""]);
				to_install.remove_all (dev_only_packages);
			}

			if (omits.contains (OPTIONAL)) {
				var optional_only_packages = compute_optional_only_packages (path_map[""]);
				to_install.remove_all (optional_only_packages);
			}

			foreach (string path in to_install.to_array ()) {
				PackageNode node = path_map[path];
				if (!node.check_compatible ())
					to_install.remove (path);
			}

			return to_install;
		}

		private void perform_reify_graph (PackageNode node, File project_root, Gee.Set<string> packages_to_install,
				Promise<bool>? prereq, Gee.List<Future<bool>> futures, Gee.List<Package> installed_packages,
				Cancellable inner) {
			Promise<bool>? my_job = prereq;

			string path = node.compute_path ();
			if (packages_to_install.contains (path)) {
				my_job = new Promise<bool> ();
				futures.add (my_job.future);

				var dir = install_dir_for_dependency (path, project_root);

				perform_download_and_unpack.begin (node, dir, my_job, prereq, installed_packages, inner);
			}

			foreach (var child in node.children.values)
				perform_reify_graph (child, project_root, packages_to_install, my_job, futures, installed_packages, inner);
		}

		private async void perform_download_and_unpack (PackageNode node, File dir, Promise<bool> job, Promise<bool>? prereq,
				Gee.List<Package> installed_packages, Cancellable inner) {
			try {
				if (prereq != null)
					yield prereq.future.wait_async (inner);

				string progress_details = "%s@%s".printf (node.name, node.version.str);

				Json.Reader? pkg = yield try_open_already_installed_manifest (node, dir, inner);
				bool was_installed = false;

				if (pkg != null) {
					install_progress (PACKAGE_ALREADY_INSTALLED, 1.0, progress_details);
				} else {
					yield download_and_unpack (node.name, node.version.str, node.resolved, dir, node.integrity,
						node.shasum, inner);
					pkg = yield load_json (dir.get_child ("package.json"), inner);
					install_progress (PACKAGE_INSTALLED, 1.0, progress_details);
					was_installed = true;
				}

				if (node.optional_peers == null)
					node.optional_peers = read_optional_peers (pkg);
				if (node.deprecated == null)
					node.deprecated = read_deprecated (pkg);
				if (node.bin == null)
					node.bin = read_bin (pkg, node.name);
				node.has_install_script = read_has_install_script (pkg);
				if (node.license == null)
					node.license = read_license (pkg);
				if (node.funding == null)
					node.funding = read_funding (pkg);

				if (was_installed) {
					string? description = read_description (pkg);
					installed_packages.add (new Package (node.name, node.version.str, description));
				}

				job.resolve (true);
			} catch (GLib.Error e) {
				job.reject (e);
			}
		}

		private static async Json.Reader? try_open_already_installed_manifest (PackageNode node, File dir, Cancellable? cancellable)
				throws IOError {
			try {
				Json.Reader r = yield load_json (dir.get_child ("package.json"), cancellable);

				r.read_member ("name");
				string? name = r.get_string_value ();
				r.end_member ();

				r.read_member ("version");
				string? version = r.get_string_value ();
				r.end_member ();

				if (name == null || version == null || name != node.name || version != node.version.str)
					return null;

				return r;
			} catch (Error e) {
				return null;
			}
		}

		private class MutationSpec {
			public MutationKind kind;
			public string name;
			public string range;
			public PackageRole role = RUNTIME;
		}

		private enum MutationKind {
			ADD,
			REMOVE,
		}

		private class PackageNode {
			public string? name;
			public SemverVersion? version;
			public PackageDependencies dependencies;
			public Gee.Set<string> optional_peers;

			public bool is_optional = false;
			public string? deprecated;
			public Gee.Map<string, string>? bin;
			public bool has_install_script = false;
			public string? license;
			public Gee.List<FundingSource>? funding;
			public Gee.Map<string, string>? engines;
			public Gee.List<string>? os;
			public Gee.List<string>? cpu;
			public Gee.List<string>? libc;

			public string? resolved;
			public string? integrity;
			public string? shasum;

			public weak PackageNode? parent;
			public Gee.Map<string, PackageNode> children = new Gee.TreeMap<string, PackageNode> ();

			public bool marked = false;

			public bool is_root {
				get {
					return parent == null;
				}
			}

			public Gee.Map<string, PackageDependency> active_deps {
				get {
					if (_active_deps == null) {
						if (parent == null) {
								_active_deps = dependencies.all;
						} else {
								_active_deps = new Gee.TreeMap<string, PackageDependency> ();
								foreach (var d in dependencies.all.values) {
										if (d.role != DEVELOPMENT)
											_active_deps[d.name] = d;
								}
						}
					}
					return _active_deps;
				}
			}

			private Gee.Map<string, PackageDependency> _active_deps;

			public PackageNode (string? name = null, SemverVersion? version = null, PackageDependencies? dependencies = null) {
				this.name = name;
				this.version = version;
				this.dependencies = (dependencies != null) ? dependencies : new PackageDependencies ();
			}

			~PackageNode () {
				foreach (var child in children.values) {
					if (child.parent == this)
						child.parent = null;
				}
			}

			public string compute_path () {
				var stack = new Gee.LinkedList<string> ();
				for (var cur = this; cur.parent != null; cur = cur.parent) {
					stack.offer_head ("node_modules/" + cur.name);
				}
				return string.joinv ("/", stack.to_array ());
			}

			public PackageNode? find_provider (string name) {
				for (var anc = this; anc != null; anc = anc.parent) {
					var cand = anc.children[name];
					if (cand != null)
						return cand;
				}
				return null;
			}

			public Gee.Map<string, string> find_missing_ranges (Manifest manifest) {
				var missing = new Gee.HashMap<string, string> ();
				foreach (var dep in manifest.dependencies.all.values) {
					if (find_provider (dep.name) == null)
						missing[dep.name] = dep.version.range;
				}
				return missing;
			}

			public bool check_compatible () throws Error {
				unowned string current_os = get_current_os ();
				if (os != null && !check_compatibility (current_os, os)) {
					if (is_optional)
						return false;
					throw new Error.NOT_SUPPORTED ("Package %s@%s is incompatible with current OS. Requires: %s, Current: %s",
						name, version.str,
						string.joinv ("/", os.to_array ()),
						current_os);
				}

				unowned string current_cpu = get_current_cpu ();
				if (cpu != null && !check_compatibility (current_cpu, cpu)) {
					if (is_optional)
						return false;
					throw new Error.NOT_SUPPORTED ("Package %s@%s is incompatible with current CPU. Requires: %s, Current: %s",
						name, version.str,
						string.joinv ("/", cpu.to_array ()),
						current_cpu);
				}

				unowned string current_libc = get_current_libc ();
				if (libc != null && !check_compatibility (current_libc, libc)) {
					if (is_optional)
						return false;
					throw new Error.NOT_SUPPORTED ("Package %s@%s is incompatible with current libc. Requires: %s, Current: %s",
						name, version.str,
						string.joinv ("/", libc.to_array ()),
						current_libc);
				}

				return true;
			}

			private static bool check_compatibility (string current, Gee.List<string> constraints) {
				foreach (string entry in constraints) {
					if (entry.has_prefix ("!")) {
						string forbidden = entry[1:];
						if (current == forbidden)
							return false;
					} else {
						if (current == entry)
							return true;
					}
				}

				bool only_negated = true;
				foreach (string entry in constraints) {
					if (!entry.has_prefix ("!")) {
						only_negated = false;
						break;
					}
				}

				return only_negated;
			}
		}

		private class Manifest {
			public string? name;
			public SemverVersion? version;
			public string? license;
			public Gee.List<FundingSource>? funding;
			public Gee.Map<string, string>? engines;
			public PackageDependencies dependencies = new PackageDependencies ();

			public string? json;
			public Json.Node root;
			public uint indent_level = 2;
			public unichar indent_char = ' ';
		}

		private class PackageVersion {
			public string range;

			public bool is_pinned {
				get {
					return range[0].isdigit ();
				}
			}

			public PackageVersion (string range) {
				this.range = range;
			}
		}

		private class PackageDependencies {
			public Gee.Map<string, PackageDependency> all = new Gee.TreeMap<string, PackageDependency> ();

			public Gee.Map<string, PackageDependency> runtime {
				get {
					if (_runtime == null)
						_runtime = compute_subset_with_role (RUNTIME);
					return _runtime;
				}
			}

			public Gee.Map<string, PackageDependency> development {
				get {
					if (_development == null)
						_development = compute_subset_with_role (DEVELOPMENT);
					return _development;
				}
			}

			public Gee.Map<string, PackageDependency> optional {
				get {
					if (_optional == null)
						_optional = compute_subset_with_role (OPTIONAL);
					return _optional;
				}
			}

			public Gee.Map<string, PackageDependency> peer {
				get {
					if (_peer == null)
						_peer = compute_subset_with_role (PEER);
					return _peer;
				}
			}

			private Gee.Map<string, PackageDependency> _runtime;
			private Gee.Map<string, PackageDependency> _development;
			private Gee.Map<string, PackageDependency> _optional;
			private Gee.Map<string, PackageDependency> _peer;

			public void add (PackageDependency d) {
				all[d.name] = d;
				clear_caches ();
			}

			public void remove (string name) {
				if (all.unset (name))
					clear_caches ();
			}

			private void clear_caches () {
				_runtime = null;
				_development = null;
				_optional = null;
				_peer = null;
			}

			private Gee.Map<string, PackageDependency> compute_subset_with_role (PackageRole role) {
				var result = new Gee.TreeMap<string, PackageDependency> ();
				foreach (PackageDependency d in all.values) {
					if (d.role == role)
						result[d.name] = d;
				}
				return result;
			}
		}

		private class PackageDependency {
			public string name;
			public PackageVersion version;
			public PackageRole role;
		}

		private class FundingSource {
			public string? type;
			public string url;
		}

		private class PackageLockPackageInfo {
			public string? name;
			public SemverVersion? version;
			public PackageDependencies dependencies = new PackageDependencies ();

			public bool is_dev = false;
			public bool is_optional = false;
			public string? deprecated;
			public Gee.Set<string> optional_peers;
			public Gee.Map<string, string>? bin;
			public bool has_install_script;
			public string? license;
			public Gee.List<FundingSource>? funding;
			public Gee.Map<string, string>? engines;
			public Gee.List<string>? os;
			public Gee.List<string>? cpu;
			public Gee.List<string>? libc;

			public string? resolved;
			public string? integrity;
		}

		private static Gee.Map<string, PackageNode> build_path_map (PackageNode root) {
			var path_map = new Gee.TreeMap<string, PackageNode> ();

			var node_stack = new Gee.LinkedList<PackageNode> ();
			var key_stack = new Gee.LinkedList<string> ();

			node_stack.offer_head (root);
			key_stack.offer_head ("");

			while (!node_stack.is_empty) {
				var node = node_stack.poll_head ();
				string key = key_stack.poll_head ();
				path_map[key] = node;

				foreach (PackageNode ch in node.children.values) {
					string child_key = (key == "") ? "" : key + "/";
					child_key += "node_modules/" + ch.name;
					node_stack.offer_head (ch);
					key_stack.offer_head (child_key);
				}
			}

			return path_map;
		}

		private static Gee.Set<string> compute_dev_only_packages (PackageNode root) {
			var dev_only = new Gee.HashSet<string> ();
			var all_packages = new Gee.HashSet<string> ();
			var non_dev_reachable = new Gee.HashSet<string> ();

			collect_all_package_paths (root, all_packages);

			mark_reachable_through_non_dev_deps (root, non_dev_reachable);

			foreach (string path in all_packages) {
				if (!non_dev_reachable.contains (path))
					dev_only.add (path);
			}

			return dev_only;
		}

		private static Gee.Set<string> compute_optional_only_packages (PackageNode root) {
			var optional_only = new Gee.HashSet<string> ();
			var all_packages = new Gee.HashSet<string> ();
			var required_packages = new Gee.HashSet<string> ();

			collect_all_package_paths (root, all_packages);

			mark_required_packages (root, required_packages);

			foreach (string path in all_packages) {
				if (!required_packages.contains (path))
					optional_only.add (path);
			}

			return optional_only;
		}

		private static void collect_all_package_paths (PackageNode node, Gee.Set<string> paths) {
			if (!node.is_root)
				paths.add (node.compute_path ());

			foreach (var child in node.children.values)
				collect_all_package_paths (child, paths);
		}

		private static void mark_required_packages (PackageNode node, Gee.Set<string> required) {
			foreach (var child in node.children.values) {
				string child_path = child.compute_path ();

				var dep = node.active_deps[child.name];
				if (dep != null && dep.role != OPTIONAL)
					required.add (child_path);

				mark_required_packages (child, required);
			}
		}

		private static void mark_reachable_through_non_dev_deps (PackageNode node, Gee.Set<string> reachable) {
			var visited = new Gee.HashSet<PackageNode> ();
			var queue = new Gee.LinkedList<PackageNode> ();

			foreach (var dep in node.active_deps.values) {
				if (dep.role != DEVELOPMENT) {
					var provider = node.find_provider (dep.name);
					if (provider != null)
						queue.offer (provider);
				}
			}

			PackageNode? current;
			while ((current = queue.poll ()) != null) {
				if (!visited.add (current))
					continue;

				string path = current.compute_path ();
				if (path != "")
					reachable.add (path);

				foreach (var dep in current.active_deps.values) {
					if (dep.role != DEVELOPMENT) {
						var provider = current.find_provider (dep.name);
						if (provider != null)
							queue.offer (provider);
					}
				}
			}
		}

		private static File install_dir_for_dependency (string path, File project_root) {
			File location = project_root;
			foreach (unowned string part in path.split ("/"))
				location = location.get_child (part);
			return location;
		}

		private class PackageDataCache {
			public Gee.Map<string, Promise<ResolvedPackageData>> data =
				new Gee.HashMap<string, Promise<ResolvedPackageData>> ();
			public Gee.Map<string, Promise<Json.Node>> packument =
				new Gee.HashMap<string, Promise<Json.Node>> ();

			private PackageManager manager;
			private Cancellable? cancellable;

			public PackageDataCache (PackageManager manager, Cancellable? cancellable) {
				this.manager = manager;
				this.cancellable = cancellable;
			}

			public Future<ResolvedPackageData> fetch (string name, PackageVersion ver) {
				string key = name + "@" + ver.range;
				Promise<ResolvedPackageData> p = data[key];
				if (p == null) {
					p = new Promise<ResolvedPackageData> ();
					data[key] = p;
					manager.fetch_and_resolve_package_data.begin (name, ver, p, packument, cancellable);
				}
				return p.future;
			}
		}

		private async void fetch_and_resolve_package_data (string name, PackageVersion version,
				Promise<ResolvedPackageData> promise_to_fulfill, Gee.Map<string, Promise<Json.Node>> packument_cache,
				Cancellable? cancellable) {
			try {
				string effective_version_local;
				Json.Reader meta_reader;

				if (Semver.is_precise_range (version.range)) {
					meta_reader = yield fetch (
						"/%s/%s".printf (Uri.escape_string (name), Uri.escape_string (version.range)),
						cancellable);

					meta_reader.read_member ("version");
					var ver_val = meta_reader.get_string_value ();
					if (ver_val == null) {
						throw new Error.PROTOCOL ("Registry 'version' for '%s@%s' missing or invalid",
							name, version.range);
					}
					effective_version_local = ver_val;
					meta_reader.end_member ();
				} else {
					Promise<Json.Node>? packument_promise = packument_cache[name];
					if (packument_promise == null) {
						packument_promise = new Promise<Json.Node> ();
						fetch_packument_for_cache.begin (name, packument_promise, cancellable);
						packument_cache[name] = packument_promise;
					}
					var packument_node = yield packument_promise.future.wait_async (cancellable);
					var packument_reader = make_json_reader_taking_node ((owned) packument_node);

					string? version_from_dist_tag = null;
					if (!packument_reader.read_member ("dist-tags"))
						throw new Error.PROTOCOL ("Registry entry missing 'dist-tags' field");
					packument_reader.read_member (version.range);
					version_from_dist_tag = packument_reader.get_string_value ();
					packument_reader.end_member ();
					packument_reader.end_member ();

					packument_reader.read_member ("versions");

					if (version_from_dist_tag != null) {
						effective_version_local = version_from_dist_tag;
					} else {
						string[]? available_version_strings = packument_reader.list_members ();
						if (available_version_strings == null || available_version_strings.length == 0) {
							throw new Error.PROTOCOL (
								"Packument 'versions' missing, not an object, or empty for '%s'", name);
						}

						string? target_version_str = Semver.max_satisfying (
							new Gee.ArrayList<string>.wrap (available_version_strings),
							version.range);
						if (target_version_str == null) {
							throw new Error.PROTOCOL ("No version satisfying '%s' for '%s'",
								version.range, name);
						}
						effective_version_local = target_version_str;
					}

					packument_reader.read_member (effective_version_local);
					meta_reader = packument_reader;

					meta_reader.read_member ("version");
					string? actual_fetched_version = meta_reader.get_string_value ();
					if (actual_fetched_version == null || actual_fetched_version != effective_version_local) {
						throw new Error.PROTOCOL ("Version object for '%s@%s' missing, invalid, or its 'version' " +
							"field is incorrect/missing. Fetched: '%s', Expected: '%s'",
							name,
							effective_version_local,
							(actual_fetched_version != null) ? actual_fetched_version : "null",
							effective_version_local);
					}
					meta_reader.end_member ();
				}

				var rpd = new ResolvedPackageData () {
					name = name,
					effective_version = Semver.parse_version (effective_version_local)
				};

				read_package_version_metadata (meta_reader, rpd);

				promise_to_fulfill.resolve (rpd);
			} catch (GLib.Error e) {
				promise_to_fulfill.reject (e);
			}
		}

		private class ResolvedPackageData {
			public string name;
			public SemverVersion effective_version;
			public string? deprecated;
			public Gee.Map<string, string>? bin;
			public bool has_install_script;
			public Gee.Set<string> optional_peers;
			public string? license;
			public Gee.List<FundingSource>? funding;
			public Gee.Map<string, string>? engines;
			public Gee.List<string>? os;
			public Gee.List<string>? cpu;
			public Gee.List<string>? libc;
			public PackageDependencies dependencies;

			public string resolved_url;
			public string? integrity;
			public string? shasum;
		}

		private async void fetch_packument_for_cache (string name, Promise<Json.Node> request, Cancellable? cancellable)
				throws Error, IOError {
			try {
				Json.Node root = yield fetch_node ("/" + Uri.escape_string (name), cancellable);
				request.resolve (root);
			} catch (GLib.Error e) {
				request.reject (e);
			}
		}

		private static void read_package_version_metadata (Json.Reader r, ResolvedPackageData rpd) throws Error {
			r.read_member ("dist");

			r.read_member ("tarball");
			rpd.resolved_url = r.get_string_value ();
			if (rpd.resolved_url == null) {
				throw new Error.PROTOCOL ("'dist.tarball' for '%s@%s' missing or invalid, or 'dist' not an object",
					rpd.name, rpd.effective_version.str);
			}
			r.end_member ();

			r.read_member ("integrity");
			rpd.integrity = r.get_string_value ();
			r.end_member ();

			r.read_member ("shasum");
			rpd.shasum = r.get_string_value ();
			r.end_member ();

			r.end_member ();

			rpd.deprecated = read_deprecated (r);
			rpd.bin = read_bin (r, rpd.name);
			rpd.has_install_script = read_has_install_script (r);
			rpd.optional_peers = read_optional_peers (r);
			rpd.license = read_license (r);
			rpd.funding = read_funding (r);
			rpd.engines = read_engines (r);
			rpd.os = read_array_str (r, "os");
			rpd.cpu = read_array_str (r, "cpu");
			rpd.libc = read_array_str (r, "libc");
			rpd.dependencies = read_dependencies (r);
		}

		private static void write_name (string? name, Json.Builder b) {
			if (name != null)
				b.set_member_name ("name").add_string_value (name);
		}

		private static void write_version (SemverVersion? version, Json.Builder b) {
			if (version != null)
				b.set_member_name ("version").add_string_value (version.str);
		}

		private static string? read_deprecated (Json.Reader r) {
			r.read_member ("deprecated");
			string? str = r.get_string_value ();
			r.end_member ();
			return str;
		}

		private static void write_deprecated (string? msg, Json.Builder b) {
			if (msg != null)
				b.set_member_name ("deprecated").add_string_value (msg);
		}

		private static Gee.Map<string, string>? read_bin (Json.Reader r, string? package_name = null) throws Error {
			if (!r.read_member ("bin")) {
				r.end_member ();
				return null;
			}

			if (r.is_object ()) {
				r.end_member ();
				var bin = read_map_str_str (r, "bin");
				foreach (var k in bin.keys.to_array ())
					bin[k] = normalize_bin_path (bin[k]);
				return bin;
			}

			if (package_name == null)
				throw new Error.PROTOCOL ("Invalid 'bin' field");
			var result = new Gee.HashMap<string, string> ();
			string? path = r.get_string_value ();
			if (path == null)
				throw new Error.PROTOCOL ("Invalid 'bin' field");
			result[package_name] = normalize_bin_path (path);
			r.end_member ();
			return result;
		}

		private static string normalize_bin_path (string path) {
			return path.has_prefix ("./")
				? path[2:]
				: path;
		}

		private static void write_bin (Gee.Map<string, string>? bin, Json.Builder b) {
			write_map_str_str (bin, "bin", b);
		}

		private static bool read_has_install_script (Json.Reader r) {
			bool has_install_script = false;
			if (r.read_member ("scripts")) {
				string[] install_script_names = { "preinstall", "install", "postinstall" };
				foreach (string name in install_script_names) {
					has_install_script = r.read_member (name);
					r.end_member ();
					if (has_install_script)
						break;
				}
			}
			r.end_member ();
			return has_install_script;
		}

		private static void write_has_install_script (bool has_install_script, Json.Builder b) {
			if (has_install_script)
				b.set_member_name ("hasInstallScript").add_boolean_value (has_install_script);
		}

		private static Gee.Set<string> read_optional_peers (Json.Reader r) throws Error {
			var result = new Gee.HashSet<string> ();
			if (!r.read_member ("peerDependenciesMeta")) {
				r.end_member ();
				return result;
			}

			string[]? members = r.list_members ();
			if (members == null)
				throw new Error.PROTOCOL ("Invalid 'peerDependenciesMeta' field");
			foreach (unowned string dep_name in members) {
				r.read_member (dep_name);
				r.read_member ("optional");
				if (r.get_boolean_value ())
					result.add (dep_name);
				r.end_member ();
				r.end_member ();
			}

			r.end_member ();

			return result;
		}

		private static void write_optional_peers (Gee.Set<string> optional_peers, Json.Builder b) {
			if (optional_peers.is_empty)
				return;
			b
				.set_member_name ("peerDependenciesMeta")
				.begin_object ();
			foreach (string dep_name in optional_peers) {
				b
					.set_member_name (dep_name)
					.begin_object ()
					.set_member_name ("optional")
					.add_boolean_value (true)
					.end_object ();
			}
			b.end_object ();
		}

		private static string? read_description (Json.Reader r) {
			r.read_member ("description");
			string? description = r.get_string_value ();
			r.end_member ();
			return description;
		}

		private static string? read_license (Json.Reader r) {
			string? license;

			r.read_member ("license");
			if (r.is_object ()) {
				r.read_member ("type");
				license = r.get_string_value ();
				r.end_member ();
			} else {
				license = r.get_string_value ();
			}
			r.end_member ();

			return license;
		}

		private static void write_license (string? license, Json.Builder b) {
			if (license != null)
				b.set_member_name ("license").add_string_value (license);
		}

		private static Gee.List<FundingSource>? read_funding (Json.Reader r) throws Error {
			if (!r.read_member ("funding")) {
				r.end_member ();
				return null;
			}

			var result = new Gee.ArrayList<FundingSource> ();

			if (r.is_array ()) {
				int n = r.count_elements ();
				for (int i = 0; i != n; i++) {
					r.read_element (i);
					read_funding_source (r, result);
					r.end_element ();
				}
			} else {
				read_funding_source (r, result);
			}

			r.end_member ();

			return result.is_empty ? null : result;
		}

		private static void write_funding (Gee.List<FundingSource>? funding, Json.Builder b) {
			if (funding == null)
				return;
			b.set_member_name ("funding");
			if (funding.size == 1) {
				write_funding_source (funding.get (0), b);
			} else {
				b.begin_array ();
				foreach (var s in funding)
					write_funding_source (s, b);
				b.end_array ();
			}
		}

		private static void read_funding_source (Json.Reader r, Gee.List<FundingSource> list) throws Error {
			if (r.is_object ()) {
				var source = new FundingSource ();

				r.read_member ("type");
				source.type = r.get_string_value ();
				r.end_member ();

				r.read_member ("url");
				source.url = r.get_string_value ();
				if (source.url == null)
					throw new Error.PROTOCOL ("Invalid 'funding' field");
				r.end_member ();

				list.add (source);
			} else {
				var val = r.get_string_value ();
				if (val == null)
					throw new Error.PROTOCOL ("Invalid 'funding' field");
				list.add (new FundingSource () { url = val });
			}
		}

		private static void write_funding_source (FundingSource s, Json.Builder b) {
			b.begin_object ();

			if (s.type != null)
				b.set_member_name ("type").add_string_value (s.type);

			b.set_member_name ("url").add_string_value (s.url);

			b.end_object ();
		}

		private static Gee.Map<string, string>? read_engines (Json.Reader r) throws Error {
			return read_map_str_str (r, "engines");
		}

		private static void write_engines (Gee.Map<string, string>? engines, Json.Builder b) {
			write_map_str_str (engines, "engines", b);
		}

		private static PackageDependencies read_dependencies (Json.Reader r) throws Error {
			var deps = new PackageDependencies ();

			string[] section_keys = { "devDependencies", "optionalDependencies", "peerDependencies", "dependencies" };
			PackageRole[] section_roles = { DEVELOPMENT, OPTIONAL, PEER, RUNTIME };
			for (uint i = 0; i != section_keys.length; i++) {
				unowned string section_key = section_keys[i];

				if (!r.read_member (section_key)) {
					r.end_member ();
					continue;
				}

				string[]? names = r.list_members ();
				if (names == null)
					throw new Error.PROTOCOL ("Invalid '%s' field", section_key);

				PackageRole role = section_roles[i];
				foreach (unowned string name in names) {
					r.read_member (name);

					string? range = r.get_string_value ();
					if (range == null)
						throw new Error.PROTOCOL ("Invalid '%s' entry for '%s'", section_key, name);

					deps.add (new PackageDependency () {
						name = name,
						version = new PackageVersion (range),
						role = role,
					});

					r.end_member ();
				}

				r.end_member ();
			}

			return deps;
		}

		private static void write_dependencies_section (string section, Gee.Map<string, PackageDependency> deps, Json.Builder b) {
			if (deps.is_empty)
				return;
			b.set_member_name (section).begin_object ();
			foreach (PackageDependency dep in deps.values)
				b.set_member_name (dep.name).add_string_value (dep.version.range);
			b.end_object ();
		}

		private static Gee.Map<string, string>? read_map_str_str (Json.Reader r, string name) throws Error {
			if (!r.read_member (name)) {
				r.end_member ();
				return null;
			}

			var result = new Gee.TreeMap<string, string> ();
			string[]? members = r.list_members ();
			if (members == null)
				throw new Error.PROTOCOL ("Invalid '%s' field", name);
			foreach (unowned string key in members) {
				r.read_member (key);
				string? val = r.get_string_value ();
				if (val == null)
					throw new Error.PROTOCOL ("Invalid '%s' value", name);
				result[key] = val;
				r.end_member ();
			}

			r.end_member ();

			return result;
		}

		private static void write_map_str_str (Gee.Map<string, string>? map, string name, Json.Builder b) {
			if (map == null)
				return;
			b.set_member_name (name).begin_object ();
			foreach (var e in map.entries)
				b.set_member_name (e.key).add_string_value (e.value);
			b.end_object ();
		}

		private static Gee.List<string>? read_array_str (Json.Reader r, string name) throws Error {
			if (!r.read_member (name)) {
				r.end_member ();
				return null;
			}

			var result = new Gee.ArrayList<string> ();

			int n = r.count_elements ();
			if (n == -1)
				throw new Error.PROTOCOL ("Invalid '%s' field", name);

			for (int i = 0; i != n; i++) {
				r.read_element (i);

				string? val = r.get_string_value ();
				if (val == null)
					throw new Error.PROTOCOL ("Invalid '%s' value", name);
				result.add (val);

				r.end_element ();
			}

			r.end_member ();

			return result;
		}

		private static void write_array_str (string name, Gee.List<string>? array, Json.Builder b) {
			if (array == null)
				return;
			b.set_member_name (name).begin_array ();
			foreach (string s in array)
				b.add_string_value (s);
			b.end_array ();
		}

		private async void download_and_unpack (string name, string version, string tarball_url, File dest_root, string? integrity,
				string? shasum, Cancellable? cancellable) throws Error, IOError {
			string progress_details = "%s@%s".printf (name, version);
			install_progress (DOWNLOADING_PACKAGE, 0.0, progress_details);

			int io_priority = Priority.DEFAULT;

			var tar_msg = new Soup.Message ("GET", tarball_url);
			InputStream http_stream;
			try {
				http_stream = yield session.send_async (tar_msg, io_priority, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Failed to GET %s: %s", tarball_url, e.message);
			}
			if (tar_msg.status_code != 200)
				throw new Error.PROTOCOL ("Failed to GET %s: HTTP %u", tarball_url, tar_msg.status_code);

			ChecksumType algo = SHA1;
			string? integrity_algo = null;
			uint8[]? integrity_digest = null;
			if (integrity != null) {
				string[] tokens = integrity.split ("-", 2);
				if (tokens.length != 2)
					throw new Error.PROTOCOL ("Invalid integrity encoding");
				integrity_algo = tokens[0];
				switch (integrity_algo) {
					case "md5":	algo = MD5;	break;
					case "sha1":	algo = SHA1;	break;
					case "sha256":	algo = SHA256;	break;
					case "sha384":	algo = SHA384;	break;
					case "sha512":	algo = SHA512;	break;
					default:
						throw new Error.PROTOCOL ("Unsupported integrity algorithm %s", integrity_algo);
				}
				integrity_digest = Base64.decode (tokens[1]);
			}
			var checksum_converter = new ChecksumConverter (algo);
			var checksum_input = new ConverterInputStream (http_stream, checksum_converter);

			var gunzip_input = new ConverterInputStream (checksum_input, new ZlibDecompressor (GZIP));
			File temp_dest_root = dest_root.get_parent ().get_child (".fpm_" + dest_root.get_basename ());
			yield FS.rmtree_async (temp_dest_root, cancellable);
			var tar_reader = new TarStreamReader (temp_dest_root);

			uint8 buffer[32 * 1024];
			size_t read_total = 0;
			size_t report_bucket = 0;
			int64 content_len = tar_msg.get_response_headers ().get_content_length ();

			while (true) {
				ssize_t n = yield gunzip_input.read_async (buffer, io_priority, cancellable);
				if (n == 0)
					break;
				if (n < 0)
					throw new IOError.FAILED ("Stream read failed");

				try {
					yield tar_reader.feed (buffer[:n], cancellable);
				} catch (Error e) {
					throw new Error.PROTOCOL ("Unable to extract tarball at %s: %s", tarball_url, e.message);
				}
				read_total += (size_t) n;
				report_bucket += (size_t) n;

				if (content_len > 0 && report_bucket >= (1 << 20)) {
					install_progress (DOWNLOADING_PACKAGE, (double) read_total / (double) content_len,
						progress_details);
					report_bucket = 0;
				}
			}

			install_progress (DOWNLOADING_PACKAGE, (content_len > 0) ? 1.0 : -1.0, progress_details);

			tar_reader.finish ();

			Error? checksum_error = null;
			if (integrity != null) {
				uint8[] actual = checksum_converter.collect_raw_digest ();
				if (actual.length != integrity_digest.length)
					checksum_error = new Error.PROTOCOL ("Bad %s digest length for %s", integrity_algo, tarball_url);
				else if (Memory.cmp (actual, integrity_digest, integrity_digest.length) != 0)
					checksum_error = new Error.PROTOCOL ("Detected %s mismatch for %s", integrity_algo, tarball_url);
			} else if (shasum != null) {
				if (checksum_converter.collect_hex_digest () != shasum.down ())
					checksum_error = new Error.PROTOCOL ("Detected shasum mismatch for %s", tarball_url);
			}
			if (checksum_error != null) {
				yield FS.rmtree_async (temp_dest_root);
				throw checksum_error;
			}

			yield FS.rmtree_async (dest_root, cancellable);
			try {
				yield temp_dest_root.move_async (dest_root, FileCopyFlags.NONE, io_priority, cancellable, null);
			} catch (GLib.Error e) {
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			}
		}

		private static File compute_project_root (PackageInstallOptions options) {
			string? project_root = options.project_root;
			if (project_root != null)
				return File.new_for_path (project_root);

			return File.new_for_path (Environment.get_current_dir ());
		}

		public PackageInstallResult install_sync (PackageInstallOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<InstallTask> ();
			task.options = options;
			return task.execute (cancellable);
		}

		private class InstallTask : PackageManagerTask<PackageInstallResult> {
			public PackageInstallOptions? options;

			protected override async PackageInstallResult perform_operation () throws Error, IOError {
				return yield parent.install (options, cancellable);
			}
		}

		private async Json.Reader fetch (string resource, Cancellable? cancellable) throws Error, IOError {
			var root = yield fetch_node (resource, cancellable);
			return make_json_reader_taking_node ((owned) root);
		}

		private async Json.Node fetch_node (string resource, Cancellable? cancellable) throws Error, IOError {
			string url = "https://%s%s".printf (registry, resource);
			install_progress (FETCHING_RESOURCE, -1.0, url);

			Bytes bytes;
			var msg = new Soup.Message ("GET", url);
			msg.request_headers.append ("Accept", "application/vnd.npm.install-v1+json; q=1.0, application/json; q=0.8, */*");
			try {
				bytes = yield session.send_and_read_async (msg, Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to GET %s: %s", url, e.message);
			}
			if (msg.status_code != 200)
				throw new Error.PROTOCOL ("Unable to GET %s: HTTP %u", url, msg.status_code);

			var parser = new Json.Parser ();
			try {
				parser.load_from_data ((string) bytes.get_data (), (ssize_t) bytes.get_size ());
			} catch (GLib.Error e) {
				throw new Error.PROTOCOL ("Unable to parse response from %s: %s", url, e.message);
			}

			return parser.steal_root ();
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class PackageManagerTask<T> : AsyncTask<T> {
			public weak PackageManager parent {
				get;
				construct;
			}
		}
	}

	/**
	 * The phases an install passes through, reported via
	 * {@link PackageManager.install_progress}.
	 */
	public enum PackageInstallPhase {
		/**
		 * Getting ready to install.
		 */
		INITIALIZING,
		/**
		 * Preparing the dependency set.
		 */
		PREPARING_DEPENDENCIES,
		/**
		 * Resolving a package to a concrete version.
		 */
		RESOLVING_PACKAGE,
		/**
		 * Fetching a resource.
		 */
		FETCHING_RESOURCE,
		/**
		 * The package was already installed.
		 */
		PACKAGE_ALREADY_INSTALLED,
		/**
		 * Downloading a package.
		 */
		DOWNLOADING_PACKAGE,
		/**
		 * A package was installed.
		 */
		PACKAGE_INSTALLED,
		/**
		 * Resolving and installing the full dependency set.
		 */
		RESOLVING_AND_INSTALLING_ALL,
		/**
		 * The installation is complete.
		 */
		COMPLETE,
	}

	/**
	 * A package available in the registry.
	 */
	public sealed class Package : Object {
		/**
		 * The package name.
		 */
		public string name {
			get;
			construct;
		}

		/**
		 * The package version.
		 */
		public string version {
			get;
			construct;
		}

		/**
		 * A short description of the package, if any.
		 */
		public string? description {
			get;
			construct;
		}

		/**
		 * A URL with more information about the package, if any.
		 */
		public string? url {
			get;
			construct;
		}

		internal Package (string name, string version, string? description, string? url = null) {
			Object (
				name: name,
				version: version,
				description: description,
				url: prettify_url (url)
			);
		}

		private static string? prettify_url (string? url) {
			if (url == null)
				return null;
			if (url.has_prefix ("https://www.npmjs.com/package/"))
				return "https://npm.im/" + url[30:];
			return url;
		}
	}

	/**
	 * The role a package plays among a project's dependencies.
	 */
	public enum PackageRole {
		/**
		 * A regular runtime dependency.
		 */
		RUNTIME,
		/**
		 * A development-only dependency.
		 */
		DEVELOPMENT,
		/**
		 * An optional dependency.
		 */
		OPTIONAL,
		/**
		 * A peer dependency.
		 */
		PEER
	}

	/**
	 * An immutable list of {@link Package} objects.
	 */
	public sealed class PackageList : Object {
		private Gee.List<Package> items;

		internal PackageList (Gee.List<Package> items) {
			this.items = items;
		}

		/**
		 * Gets the number of packages in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the package at the given position.
		 *
		 * @param index zero-based position
		 * @return the package
		 */
		public new Package get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Options for {@link PackageManager.search}.
	 */
	public class PackageSearchOptions : Object {
		/**
		 * The number of results to skip, for paging.
		 */
		public uint offset {
			get;
			set;
			default = 0;
		}

		/**
		 * The maximum number of results to return.
		 */
		public uint limit {
			get;
			set;
			default = 20;
		}
	}

	/**
	 * The result of a {@link PackageManager.search}.
	 */
	public sealed class PackageSearchResult : Object {
		/**
		 * The packages on this page of results.
		 */
		public PackageList packages {
			get;
			construct;
		}

		/**
		 * The total number of matching packages, across all pages.
		 */
		public uint total {
			get;
			construct;
		}

		internal PackageSearchResult (PackageList packages, uint total) {
			Object (packages: packages, total: total);
		}
	}

	/**
	 * Options for {@link PackageManager.install}.
	 */
	public class PackageInstallOptions : Object {
		internal Gee.List<string> specs = new Gee.ArrayList<string> ();
		internal Gee.Set<PackageRole> omits = new Gee.HashSet<PackageRole> ();

		/**
		 * The project directory to install into, or null for the current
		 * directory.
		 */
		public string? project_root {
			get;
			set;
		}

		/**
		 * The role to record the installed packages under.
		 */
		public PackageRole role {
			get;
			set;
			default = RUNTIME;
		}

		/**
		 * Removes all package specs to install.
		 */
		public void clear_specs () {
			specs.clear ();
		}

		/**
		 * Adds a package spec to install, such as `foo` or `foo@1.2.3`.
		 *
		 * @param spec the package spec
		 */
		public void add_spec (string spec) {
			specs.add (spec);
		}

		/**
		 * Removes all omitted roles.
		 */
		public void clear_omits () {
			omits.clear ();
		}

		/**
		 * Omits dependencies of the given role from the installation.
		 *
		 * @param role the role to omit
		 */
		public void add_omit (PackageRole role) {
			omits.add (role);
		}
	}

	/**
	 * The result of a {@link PackageManager.install}.
	 */
	public sealed class PackageInstallResult : Object {
		/**
		 * The packages that were installed.
		 */
		public PackageList packages {
			get;
			construct;
		}

		internal PackageInstallResult (PackageList packages) {
			Object (packages: packages);
		}
	}

	private class ChecksumConverter : Object, Converter {
		private ChecksumType algo;
		private Checksum? checksum;

		public ChecksumConverter (ChecksumType algo) {
			this.algo = algo;
			reset ();
		}

		public ConverterResult convert (uint8[] inbuf, uint8[] outbuf, ConverterFlags flags, out size_t bytes_read,
				out size_t bytes_written) throws GLib.Error {
			var n = size_t.min (inbuf.length, outbuf.length);
			bytes_read = n;
			bytes_written = n;

			if (n == 0)
				return FINISHED;

			Memory.copy (outbuf, inbuf, n);
			checksum.update (inbuf, n);

			return CONVERTED;
		}

		public void reset () {
			checksum = new Checksum (algo);
		}

		public uint8[] collect_raw_digest () {
			uint8 digest[64];
			size_t len = digest.length;
			checksum.get_digest (digest, ref len);

			reset ();

			return digest[:len];
		}

		public string collect_hex_digest () {
			string digest = checksum.get_string ();
			reset ();
			return digest;
		}
	}

	private void detect_indent (string src, out uint indent_level, out unichar indent_char) {
		indent_level = 2;
		indent_char = ' ';

		Regex indents_before_first_key = /^([ \t]+)"/m;
		MatchInfo? m;
		if (indents_before_first_key.match (src, 0, out m)) {
			string seq = m.fetch (1);
			indent_level = seq.length;
			indent_char = seq.get_char (0);
		}
	}

	private string generate_npm_style_json (Json.Node root_node, uint indent_level, unichar indent_char) {
		var gen = new Json.Generator ();
		gen.set_pretty (true);
		gen.set_indent (indent_level);
		gen.set_indent_char (indent_char);
		gen.set_root (root_node);
		string pretty_json = gen.to_data (null);

		string[] lines = pretty_json.split ("\n");
		var modified_lines = new Gee.ArrayList<string> ();
		foreach (string line in lines) {
			int idx = line.index_of (" : ");
			if (idx != -1 && line[idx - 1] == '"') {
					string part1 = line[:idx];
					string part2 = line[idx + 3:];
					modified_lines.add (part1 + ": " + part2);
			} else {
				modified_lines.add (line);
			}
		}

		return string.joinv ("\n", modified_lines.to_array ()) + "\n";
	}

	private async Json.Reader load_json (File f, Cancellable? cancellable) throws Error, IOError {
		Bytes b = yield FS.read_all_bytes (f, cancellable);
		return parse_json (b);
	}

	private Json.Reader parse_json (Bytes b) throws Error {
		var p = new Json.Parser ();
		try {
			p.load_from_data ((string) b.get_data (), (ssize_t) b.get_size ());
		} catch (GLib.Error e) {
			throw new Error.PROTOCOL ("%s", e.message);
		}
		return make_json_reader_taking_node (p.steal_root ());
	}

	private class SemverVersion {
		public uint major;
		public uint minor;
		public uint patch;
		public string? prerelease;
		public string? metadata;

		public string str {
			get {
				if (_str == null) {
					var s = new StringBuilder.sized (8);
					s.append_printf ("%u.%u.%u", major, minor, patch);
					if (prerelease != null)
						s.append_c ('-').append (prerelease);
					if (metadata != null)
						s.append_c ('+').append (metadata);
					_str = s.str;
				}
				return _str;
			}
		}

		private string? _str;

		public SemverVersion (uint major, uint minor = 0, uint patch = 0, string? prerelease = null, string? metadata = null) {
			this.major = major;
			this.minor = minor;
			this.patch = patch;
			this.prerelease = prerelease;
			this.metadata = metadata;
		}
	}

	namespace Semver {
		private SemverVersion parse_version (string version) throws Error {
			string v = version;
			string? meta = null;
			int plus = version.index_of_char ('+');
			if (plus != -1) {
				v = version[:plus];
				meta = version[plus + 1:];
			}

			string core;
			string? pre = null;
			int dash = v.index_of_char ('-');
			if (dash != -1) {
				core = v[:dash];
				pre = v[dash + 1:];
			} else {
				core = v;
			}

			string[] nums = core.split (".");
			if (nums.length == 0 || nums.length > 3) {
				throw new Error.PROTOCOL ("Invalid semver version '%s': core part must have 1 to 3 segments, found %d",
					version, nums.length);
			}

			uint major = 0;
			uint minor = 0;
			uint patch = 0;

			try {
				major = parse_uint (nums[0]);
			} catch (Error e) {
				throw new Error.PROTOCOL ("Invalid major version in '%s': %s", version, e.message);
			}

			if (nums.length > 1) {
				try {
					minor = parse_uint (nums[1]);
				} catch (Error e) {
					throw new Error.PROTOCOL ("Invalid minor version in '%s': %s", version, e.message);
				}
			}

			if (nums.length > 2) {
				try {
					patch = parse_uint (nums[2]);
				} catch (Error e) {
					throw new Error.PROTOCOL ("Invalid patch version in '%s': %s", version, e.message);
				}
			}

			if (pre != null) {
				if (pre.length == 0) {
					throw new Error.PROTOCOL ("Pre-release part of '%s' cannot be empty if specified by a hyphen",
						version);
				}

				string[] pre_ids = pre.split (".");
				foreach (string id in pre_ids) {
					if (id.length == 0) {
						throw new Error.PROTOCOL ("Pre-release identifier in '%s' cannot be empty (found in '%s')",
							version, pre);
					}

					bool is_potentially_numeric = true;
					foreach (unichar c in id) {
						if (!c.isalnum () && c != '-') {
							throw new Error.PROTOCOL (
								"Pre-release identifier '%s' in '%s' contains invalid character '%s'",
								id, version, c.to_string ());
						}
						if (!c.isdigit ())
							is_potentially_numeric = false;
					}

					if (is_potentially_numeric && id.length > 1 && id[0] == '0') {
						throw new Error.PROTOCOL (
							"Numeric pre-release identifier '%s' in '%s' must not have leading zeros",
							id, version);
					}
				}
			}

			if (meta != null) {
				if (meta.length == 0) {
					throw new Error.PROTOCOL ("Build metadata part of '%s' cannot be empty if specified by a plus",
						version);
				}

				string[] meta_ids = meta.split (".");
				foreach (string id in meta_ids) {
					if (id.length == 0) {
						throw new Error.PROTOCOL (
							"Build metadata identifier in '%s' cannot be empty (found in '%s')", version, meta);
					}
					foreach (unichar c_char in id) {
						if (!c_char.isalnum () && c_char != '-') {
							throw new Error.PROTOCOL (
								"Build metadata identifier '%s' in '%s' contains invalid character '%s'",
								id, version, c_char.to_string ());
						}
					}
				}
			}

			return new SemverVersion (major, minor, patch, pre, meta);
		}

		private string extract_core_part (string version) {
			string core_part = version;

			int dash_idx = core_part.index_of_char ('-');
			if (dash_idx != -1)
				core_part = core_part[:dash_idx];

			int plus_idx = core_part.index_of_char ('+');
			if (plus_idx != -1)
				core_part = core_part[:plus_idx];

			return core_part;
		}

		private int compare_version (SemverVersion a, SemverVersion b) {
			if (a.major != b.major)
				return a.major > b.major ? 1 : -1;

			if (a.minor != b.minor)
				return a.minor > b.minor ? 1 : -1;

			if (a.patch != b.patch)
				return a.patch > b.patch ? 1 : -1;

			bool a_has_prerelease = a.prerelease != null;
			bool b_has_prerelease = b.prerelease != null;

			if (!a_has_prerelease && !b_has_prerelease)
				return 0;
			if (!a_has_prerelease)
				return 1;
			if (!b_has_prerelease)
				return -1;

			string[] a_ids = a.prerelease.split (".");
			string[] b_ids = b.prerelease.split (".");

			uint min_len = uint.min (a_ids.length, b_ids.length);
			for (uint i = 0; i != min_len; i++) {
				string id_a = a_ids[i];
				string id_b = b_ids[i];

				bool a_is_num = is_numeric_identifier (id_a);
				bool b_is_num = is_numeric_identifier (id_b);

				if (a_is_num && b_is_num) {
					int len_cmp = id_a.length - id_b.length;
					if (len_cmp != 0)
						return len_cmp;
					int digit_cmp = strcmp (id_a, id_b);
					if (digit_cmp != 0)
						return digit_cmp;
				} else if (a_is_num) {
					return -1;
				} else if (b_is_num) {
					return 1;
				} else {
					int cmp = strcmp (id_a, id_b);
					if (cmp != 0)
						return cmp;
				}
			}

			if (a_ids.length != b_ids.length)
				return a_ids.length > b_ids.length ? 1 : -1;

			return 0;
		}

		private bool is_precise_range (string range) throws Error {
			if (range == "latest")
				return true;

			if (range.index_of_char (' ') != -1 ||
					range.index_of_char ('^') != -1 ||
					range.index_of_char ('~') != -1 ||
					range.index_of_char ('>') != -1 ||
					range.index_of_char ('<') != -1 ||
					range.index_of_char ('*') != -1) {
				return false;
			}

			try {
				parse_version (range);

				return count_char (extract_core_part (range), '.') == 2;
			} catch (Error e) {
				return false;
			}
		}

		private string? max_satisfying (Gee.Collection<string> versions, string range) throws Error {
			string? best_str = null;
			SemverVersion? best_ver = null;

			foreach (string v in versions) {
				SemverVersion cand = parse_version (v);

				if (!satisfies_range (cand, range))
					continue;

				if (best_ver == null || compare_version (cand, best_ver) > 0) {
					best_ver = cand;
					best_str = v;
				}
			}

			return best_str;
		}

		private bool satisfies_range (SemverVersion cand, string range) throws Error {
			string r = range.strip ();
			if (r == "")
				throw new Error.PROTOCOL ("Invalid version range: '%s'", range);

			string[] ors = r.split ("||");

			foreach (string clause in ors) {
				string part = clause.strip ();

				if (part.index_of (" - ") != -1) {
					if (check_comparator (cand, part))
						return true;
					continue;
				}

				string[] comps = part.split (" ");
				bool ok = true;

				foreach (string comp in comps) {
					string c = comp.strip ();
					if (c == "")
						continue;

					if (!check_comparator (cand, c)) {
						ok = false;
						break;
					}
				}

				if (!ok)
					continue;

				if (cand.prerelease != null && !range_allows_prerelease (cand, clause))
					continue;

				return true;
			}

			return false;
		}

		private bool range_allows_prerelease (SemverVersion cand, string range) throws Error {
			foreach (string clause in range.split ("||")) {
				foreach (string raw_comp in clause.strip ().split (" ")) {
					string comp = raw_comp.strip ();
					if (comp == "")
						continue;

					string[] pieces = comp.split (" - ", 2);
					foreach (string p in pieces) {
						string vstr = p.strip ();

						if (vstr.has_prefix (">=") || vstr.has_prefix ("<="))
							vstr = vstr[2:];
						else if (vstr[0] == '^' || vstr[0] == '~' || vstr[0] == '>' || vstr[0] == '<')
							vstr = vstr[1:];

						string core_part = extract_core_part (vstr);
						if (core_part.index_of_char ('*') != -1 || core_part.down ().index_of_char ('x') != -1)
							continue;

						SemverVersion v = parse_version (vstr);

						if (v.prerelease != null &&
								v.major == cand.major &&
								v.minor == cand.minor &&
								v.patch == cand.patch) {
							return true;
						}
					}
				}
			}
			return false;
		}

		private bool check_comparator (SemverVersion cand, string comparator) throws Error {
			string comp = comparator.strip ();

			if (comp.index_of (" - ") != -1) {
				string[] parts = comp.split (" - ", 2);
				string lo = parts[0].strip ();
				string hi = parts[1].strip ();

				return satisfies_range (cand, ">=" + lo) && satisfies_range (cand, "<=" + hi);
			}

			if (comp == "*" || comp == "x" || comp == "X")
				return true;

			string op = "";
			string ver = comp;

			if (comp.has_prefix (">=") || comp.has_prefix ("<=")) {
				op = comp[:2];
				ver = comp[2:];
			} else if (comp[0] == '>' || comp[0] == '<') {
				op = comp[:1];
				ver = comp[1:];
			}

			if (op != "") {
				SemverVersion rv = parse_version (ver);
				int cmp = compare_version (cand, rv);

				switch (op) {
					case ">":
						return cmp > 0;
					case ">=":
						return cmp >= 0;
					case "<":
						return cmp < 0;
					case "<=":
						return cmp <= 0;
				}
			}

			if (comp.has_prefix ("~")) {
				SemverVersion lower = parse_version (comp[1:]);
				SemverVersion upper;

				if (lower.major == 0 && lower.minor == 0)
					upper = new SemverVersion (0, 0, lower.patch + 1);
				else if (lower.major == 0)
					upper = new SemverVersion (0, lower.minor + 1);
				else
					upper = new SemverVersion (lower.major, lower.minor + 1);

				return compare_version (cand, lower) >= 0 && compare_version (cand, upper) < 0;
			}

			if (comp.has_prefix ("^")) {
				SemverVersion lower = parse_version (comp[1:]);
				SemverVersion upper;

				if (lower.major != 0) {
					upper = new SemverVersion (lower.major + 1);
				} else if (lower.minor != 0) {
					upper = new SemverVersion (0, lower.minor + 1);
				} else {
					upper = new SemverVersion (0, 0, lower.patch + 1);
				}

				return compare_version (cand, lower) >= 0 && compare_version (cand, upper) < 0;
			}

			if (comp.index_of_char ('*') != -1 ||
					comp.index_of_char ('x') != -1 ||
					comp.index_of_char ('X') != -1 ||
					count_char (comp, '.') < 2) {
				return wildcard_match (cand, comp);
			}

			SemverVersion ev = parse_version (comp);
			return compare_version (cand, ev) == 0;
		}

		private bool wildcard_match (SemverVersion cand, string pat) throws Error {
			string norm = pat.replace ("*", "x").replace ("X", "x");
			string[] parts = norm.split (".");

			bool major_wild = false;
			bool minor_wild = false;
			bool patch_wild = false;

			uint major = 0;
			uint minor = 0;
			uint patch = 0;

			if (parts.length > 0) {
				if (parts[0] == "x" || parts[0] == "")
					major_wild = true;
				else
					major = parse_uint (parts[0]);
			}

			if (parts.length > 1) {
				if (parts[1] == "x" || parts[1] == "")
					minor_wild = true;
				else
					minor = parse_uint (parts[1]);
			} else {
				minor_wild = true;
			}

			if (parts.length > 2) {
				if (parts[2] == "x" || parts[2] == "")
					patch_wild = true;
				else
					patch = parse_uint (parts[2]);
			} else {
				patch_wild = true;
			}

			if (major_wild)
				return true;

			SemverVersion lower = new SemverVersion (major,
					minor_wild ? 0 : minor,
					patch_wild ? 0 : patch);

			SemverVersion upper;

			if (minor_wild)
				upper = new SemverVersion (major + 1);
			else if (patch_wild)
				upper = new SemverVersion (major, minor + 1);
			else
				return compare_version (cand, lower) == 0;

			return compare_version (cand, lower) >= 0 && compare_version (cand, upper) < 0;
		}

		private uint parse_uint (string s) throws Error {
			if (s.length == 0)
				throw new Error.PROTOCOL ("Numeric component cannot be empty");

			if (s.length > 1 && s[0] == '0')
				throw new Error.PROTOCOL ("Numeric component '%s' has leading zeros", s);

			uint result_val;
			string unparsed_str;
			if (uint.try_parse (s, out result_val, out unparsed_str)) {
				if (unparsed_str.length == 0)
					return result_val;
				throw new Error.PROTOCOL ("Numeric component '%s' contains trailing non-digit characters: \"%s\"",
					s, unparsed_str);
			} else {
				if (unparsed_str.length == 0) {
					throw new Error.PROTOCOL ("Numeric component '%s' is too large or invalid", s);
				} else {
					throw new Error.PROTOCOL ("Invalid characters in numeric component '%s': problem starts at \"%s\"",
						s, unparsed_str);
				}
			}
		}

		private bool is_numeric_identifier (string s) {
			if (s.length == 0)
				return false;
			foreach (unichar c in s) {
				if (!c.isdigit ())
					return false;
			}
			return true;
		}

		private int count_char (string s, char ch) {
			int n = 0;
			foreach (unichar c in s) {
				if (c == ch)
					n++;
			}
			return n;
		}
	}

	private class TarStreamReader : Object {
		private const size_t BLOCK_SIZE = 512;

		private File root;
		private BufferBuilder header_builder = new BufferBuilder ();
		private File? current_file = null;
		private uint32 current_file_mode = 0;
		private OutputStream? out_stream = null;
		private uint64 remaining = 0;
		private size_t pad = 0;

		public TarStreamReader (File root) {
			this.root = root;
		}

		public async void feed (uint8[] data, Cancellable? cancellable) throws Error, IOError {
			int io_priority = Priority.DEFAULT;

			size_t off = 0;
			size_t len = data.length;
			while (off < len) {
				if (remaining != 0) {
					size_t chunk_size = size_t.min ((size_t) remaining, len - off);
					if (out_stream != null) {
						try {
							yield out_stream.write_all_async (data[off:off + chunk_size], io_priority,
								cancellable, null);
						} catch (GLib.Error e) {
							throw new Error.TRANSPORT ("%s", e.message);
						}
					}
					remaining -= chunk_size;
					off += chunk_size;

					if (remaining == 0) {
						if (out_stream != null) {
							try {
								yield out_stream.close_async (io_priority, cancellable);
							} catch (GLib.Error e) {
								throw new Error.TRANSPORT ("%s", e.message);
							}

#if !WINDOWS
							try {
								var info = new FileInfo ();
								info.set_attribute_uint32 (FileAttribute.UNIX_MODE,
									current_file_mode & 0777);
								yield current_file.set_attributes_async (info, FileQueryInfoFlags.NONE,
									Priority.DEFAULT, cancellable, out info);
							} catch (GLib.Error e) {
							}
#endif

							current_file = null;
							out_stream = null;
						}
					}

					continue;
				}

				if (pad != 0) {
					size_t skip = size_t.min (pad, len - off);
					pad -= skip;
					off += skip;
					continue;
				}

				size_t n_header_bytes_missing = BLOCK_SIZE - header_builder.offset;
				if (n_header_bytes_missing != 0) {
					size_t chunk_size = size_t.min (n_header_bytes_missing, len - off);
					header_builder.append_data (data[off:off + chunk_size]);
					off += chunk_size;
					if (chunk_size != n_header_bytes_missing)
						return;
				}

				var header = new Buffer (header_builder.build ());
				header_builder = new BufferBuilder ();

				string name = header.read_fixed_string (0, 100);
				if (name.length == 0) {
					off = len;
					break;
				}
				string safe_entry = sanitize_entry (name, root);

				string mode_field = header.read_fixed_string (100, 8).split (" ")[0];
				uint32 file_mode = 0;
				if (!uint.try_parse (mode_field, out file_mode, null, 8))
					throw new Error.PROTOCOL ("Invalid tarball mode (file '%s' corrupt)", name);

				string size_field = header.read_fixed_string (124, 12).split (" ")[0];
				uint64 file_size = 0;
				if (!uint64.try_parse (size_field, out file_size, null, 8))
					throw new Error.PROTOCOL ("Invalid tarball size (file '%s' corrupt)", name);

				var typeflag = (char) header.read_uint8 (156);

				if ((typeflag == '0' || typeflag == '\0') && safe_entry != "") {
					current_file = root.get_child (safe_entry);
					current_file_mode = file_mode;
					FS.mkdirp (current_file.get_parent (), cancellable);
					try {
						out_stream = yield current_file.replace_async (null, false, FileCreateFlags.NONE,
							io_priority, cancellable);
					} catch (GLib.Error e) {
						throw new Error.TRANSPORT ("%s", e.message);
					}
				} else {
					current_file = null;
					current_file_mode = 0;
					out_stream = null;
				}
				remaining = file_size;

				pad = (size_t) ((BLOCK_SIZE - (file_size % BLOCK_SIZE)) % BLOCK_SIZE);
			}
		}

		public void finish () throws Error {
			if (current_file != null && remaining != 0)
				throw new Error.PROTOCOL ("Truncated tarball (file '%s' incomplete)", current_file.get_path ());
		}

		private static string sanitize_entry (string name, File root) throws Error {
			int slash = name.index_of ("/");
			string entry = (slash == -1) ? "" : name[slash + 1:];
			if (entry == "")
				return entry;

			entry = entry.replace ("/", Path.DIR_SEPARATOR_S);

			if (entry[0] == Path.DIR_SEPARATOR || (entry.length >= 2 && entry[1] == ':'))
				throw new Error.PROTOCOL ("Absolute path in tar: %s", name);

			foreach (string part in entry.split (Path.DIR_SEPARATOR_S)) {
				if (part == "..")
					throw new Error.PROTOCOL ("Path traversal in tar: %s", name);
			}

			string final_path = root.resolve_relative_path (entry).get_path ();
			if (!final_path.has_prefix (root.get_path () + Path.DIR_SEPARATOR_S))
				throw new Error.PROTOCOL ("Escapes extraction root: %s".printf (name));
			return entry;
		}
	}

	private unowned string get_current_os () {
#if WINDOWS
		return "win32";
#elif DARWIN
		return "darwin";
#elif ANDROID
		return "android";
#elif LINUX
		return "linux";
#elif FREEBSD
		return "freebsd";
#elif QNX
		return "qnx";
#else
		return FIXME;
#endif
	}

	private unowned string get_current_cpu () {
#if X86
		return "ia32";
#elif X86_64
		return "x64";
#elif ARM
		return "arm";
#elif ARM64
		return "arm64";
#elif MIPS
# if LITTLE_ENDIAN
		return "mipsel";
# else
		return "mips";
# endif
#else
		return FIXME;
#endif
	}

	private unowned string get_current_libc () {
#if GLIBC
		return "glibc";
#elif UCLIBC
		return "uclibc";
#elif MUSL
		return "musl";
#else
		return "default";
#endif
	}
}
