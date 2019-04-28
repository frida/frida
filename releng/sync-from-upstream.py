import os
from pathlib import Path
import re
import subprocess
import sys


def make_gnome_url(repo_name):
    return "https://gitlab.gnome.org/GNOME/{}.git".format(repo_name)


upstreams = {
    "meson": "https://github.com/mesonbuild/meson.git",
    "zlib": "https://github.com/madler/zlib.git",
    "libffi": "https://github.com/libffi/libffi.git",
    "glib": make_gnome_url("glib"),
    "glib-openssl": make_gnome_url("glib-openssl"),
    "glib-schannel": "https://github.com/centricular/glib-schannel.git",
    "libgee": make_gnome_url("libgee"),
    "json-glib": make_gnome_url("json-glib"),
    "libpsl": "https://github.com/rockdaboot/libpsl.git",
    "libxml2": make_gnome_url("libxml2"),
    "libsoup": make_gnome_url("libsoup"),
    "vala": make_gnome_url("vala"),
    "pkg-config": "https://anongit.freedesktop.org/git/pkg-config.git",
    "duktape": "https://github.com/svaarala/duktape.git",
    "v8": "https://chromium.googlesource.com/v8/v8",
    "capstone": "https://github.com/aquynh/capstone.git",
}


def sync(repo_path):
    repo_name = os.path.basename(repo_path)

    patches_path = os.path.join(str(Path.home()), ".frida-sync-" + re.sub(r"[^\w\d]", "-", repo_path.lower()).lstrip("-"))
    if os.path.exists(patches_path):
        patches = PendingPatches.load(patches_path)

        print("Applying {} pending patches".format(patches.count))
    else:
        upstream_url = upstreams.get(repo_name, None)
        if upstream_url is None:
            raise UnknownUpstreamError("Unknown upstream: {}".format(repo_name))

        print("Synchronizing with {}".format(upstream_url))

        subprocess.run(["git", "checkout", "master"], cwd=repo_path, capture_output=True, check=True)
        subprocess.run(["git", "pull"], cwd=repo_path, capture_output=True, check=True)
        result = subprocess.run(["git", "status"], cwd=repo_path, capture_output=True, check=True, encoding='utf-8')
        if not "working tree clean" in result.stdout:
            raise WorkingTreeDirtyError("Working tree is dirty")

        subprocess.run(["git", "remote", "add", "upstream", upstream_url], cwd=repo_path, capture_output=True)
        subprocess.run(["git", "fetch", "upstream"], cwd=repo_path, check=True)

        patches, base = list_our_patches(repo_path)
        print("We have {} patches on top of upstream".format(patches.count))

        new_entries = list_upstream_changes(repo_path, base)
        if len(new_entries) == 0:
            print("Already up-to-date")
            return

        print("Upstream has {} new commits".format(len(new_entries)))

        print("Merging...")
        subprocess.run(["git", "merge", "-s", "ours", "upstream/master"], cwd=repo_path, capture_output=True, check=True)
        subprocess.run(["git", "checkout", "--detach", "upstream/master"], cwd=repo_path, capture_output=True, check=True)
        subprocess.run(["git", "reset", "--soft", "master"], cwd=repo_path, capture_output=True, check=True)
        subprocess.run(["git", "checkout", "master"], cwd=repo_path, capture_output=True, check=True)
        subprocess.run(["git", "commit", "--amend", "-C", "HEAD"], cwd=repo_path, capture_output=True, check=True)

        patches.save(patches_path)

    while True:
        index, cid, message = patches.try_pop()
        if index is None:
            break

        print("Cherry-picking {}/{}: {}".format(index + 1, patches.count, message))
        try:
            subprocess.run(["git", "cherry-pick", cid], cwd=repo_path, capture_output=True, encoding='utf-8', check=True)
        except subprocess.CalledProcessError as e:
            patches.save(patches_path)

            print("\n*** Unable to apply this patch:")
            print(e.stderr)
            print("Run `git cherry-pick --abort` and re-run script to skip it.")

            return

    os.remove(patches_path)
    print("Done!")

def list_our_patches(repo_path):
    items = []
    base = None
    entries = list_recent_commits(repo_path, "--max-count=1000")
    for index, entry in enumerate(entries):
        cid, message = entry
        if message.startswith("Merge"):
            base = entries[index + 1][0]
            break
        items.append(("pending", cid, message))
    items.reverse()
    return (PendingPatches(items), base)

def list_upstream_changes(repo_path, since):
    return list(reversed(list_recent_commits(repo_path, since + "..upstream/master")))[:-1]

def list_recent_commits(repo_path, *args):
    result = subprocess.run(["git", "log", "--pretty=oneline", "--abbrev-commit", "--topo-order"] + list(args),
        cwd=repo_path, capture_output=True, check=True, encoding='utf-8', errors='surrogateescape')
    return [line.split(" ", 1) for line in result.stdout.rstrip().split("\n")]


class PendingPatches(object):
    def __init__(self, items):
        self._items = items

        offset = 0
        for status, cid, message in items:
            if status == "applied":
                offset += 1
            else:
                break
        self._offset = offset

    @property
    def count(self):
        return len(self._items)

    def try_pop(self):
        index = self._offset
        if index == len(self._items):
            return (None, None, None)

        _, cid, message = self._items[index]
        self._items[index] = ("applied", cid, message)
        self._offset += 1

        return (index, cid, message)

    @classmethod
    def load(cls, path):
        with open(path, "r", encoding='utf-8') as f:
            data = f.read()

        items = []
        for line in data.strip().split("\n"):
            status, cid, message = line.split(" ", maxsplit=2)
            items.append((status, cid, message))
        return PendingPatches(items)

    def save(self, path):
        data = "\n".join([" ".join(item) for item in self._items]) + "\n"
        with open(path, "w", encoding='utf-8') as f:
            f.write(data)


class WorkingTreeDirtyError(Exception):
    pass


class UnknownUpstreamError(Exception):
    pass


if __name__ == '__main__':
    sync(os.path.abspath(sys.argv[1]))
