import os
import subprocess
import sys


def make_gnome_url(repo_name):
    return "https://gitlab.gnome.org/GNOME/{}.git".format(repo_name)


upstreams = {
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
    "v8": "https://chromium.googlesource.com/v8/v8",
}


def sync(repo_path):
    repo_name = os.path.basename(repo_path)

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
    print("We have {} patches on top of upstream".format(len(patches)))

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

    for index, entry in enumerate(patches):
        cid, message = entry
        print("Cherry-picking {}/{}: {}".format(index + 1, len(patches), message))
        subprocess.run(["git", "cherry-pick", cid], cwd=repo_path, capture_output=True, check=True)

    print("Done!")

def list_our_patches(repo_path):
    patches = []
    base = None
    entries = list_recent_commits(repo_path, "--max-count=1000")
    for index, entry in enumerate(entries):
        cid, message = entry
        if message.startswith("Merge"):
            base = entries[index + 1][0]
            break
        patches.append((cid, message))
    patches.reverse()
    return (patches, base)

def list_upstream_changes(repo_path, since):
    return list(reversed(list_recent_commits(repo_path, since + "..upstream/master")))[:-1]

def list_recent_commits(repo_path, *args):
    result = subprocess.run(["git", "log", "--pretty=oneline", "--abbrev-commit", "--topo-order"] + list(args),
        cwd=repo_path, capture_output=True, check=True, encoding='utf-8')
    return [line.split(" ", 1) for line in result.stdout.rstrip().split("\n")]



class WorkingTreeDirtyError(Exception):
    pass

class UnknownUpstreamError(Exception):
    pass


if __name__ == '__main__':
    sync(os.path.abspath(sys.argv[1]))
