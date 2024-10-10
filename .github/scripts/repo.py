#!/usr/bin/env python3

import argparse
from configparser import ConfigParser
from pathlib import Path
import subprocess
import sys
from typing import Iterator

ROOT_DIR = Path(__file__).parent.parent.parent.resolve()
RELENG_DIR = ROOT_DIR / "releng"
if not (RELENG_DIR / "meson" / "meson.py").exists():
    subprocess.run(["git", "submodule", "update", "--init", "--depth", "1", "--recursive", "releng"],
                   cwd=ROOT_DIR)
sys.path.insert(0, str(ROOT_DIR))
from releng.deps import load_dependency_parameters, query_repo_commits


PROJECT_NAMES_IN_RELEASE_CYCLE = [
    "frida-gum",
    "frida-core",
    "frida-clr",
    "frida-node",
    "frida-python",
    "frida-qml",
    "frida-swift",
]


def main(argv: list[str]):
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    command = subparsers.add_parser("bump", help="bump all the things")
    command.set_defaults(func=lambda args: bump())

    command = subparsers.add_parser("tag", help="tag a new release")
    command.add_argument("version")
    command.set_defaults(func=lambda args: tag(args.version))

    command = subparsers.add_parser("backtag", help="retroactively tag an old release")
    command.add_argument("version")
    command.set_defaults(func=lambda args: backtag(args.version))

    args = parser.parse_args()
    if "func" in args:
        try:
            args.func(args)
        except Exception as e:
            print(e, file=sys.stderr)
            if isinstance(e, subprocess.CalledProcessError):
                for label, data in [("Output", e.output),
                                    ("Stderr", e.stderr)]:
                    if data:
                        print(f"{label}:\n\t| " + "\n\t| ".join(data.strip().split("\n")), file=sys.stderr)
            sys.exit(1)
    else:
        parser.print_usage(file=sys.stderr)
        sys.exit(1)


def bump():
    projects = list(enumerate_projects_in_release_cycle())
    projects.append(("frida-tools", ROOT_DIR / "subprojects" / "frida-tools"))

    assert_no_local_changes(ROOT_DIR)
    for _, repo in projects:
        assert_no_local_changes(repo)

    print("# releng")
    bump_releng(ROOT_DIR / "releng")
    if query_local_changes(ROOT_DIR):
        print("\tbumped")
    else:
        print("\tup-to-date")

    for name, repo in projects:
        bump_subproject(name, repo)

    if bump_submodules():
        push_changes("frida", ROOT_DIR)


def bump_subproject(name: str, repo: Path):
    print(f"# {name}")

    if not (repo / "meson.build").exists():
        run(["git", "submodule", "update", "--init", "--depth", "1", Path("subprojects") / repo], cwd=ROOT_DIR)
    run(["git", "checkout", "main"], cwd=repo)
    run(["git", "pull"], cwd=repo)

    releng = repo / "releng"
    bump_releng(releng)
    if query_local_changes(repo):
        run(["git", "submodule", "update"], cwd=releng)
        run(["git", "add", "releng"], cwd=repo)
        run(["git", "commit", "-m", "submodules: Bump releng"], cwd=repo)
        print("\treleng: bumped")
    else:
        print("\treleng: up-to-date")

    bumped_files: list[Path] = []
    dep_packages = load_dependency_parameters().packages
    for identifier, config, wrapfile in enumerate_git_wraps_in_repo(repo):
        if identifier == "nan":
            continue

        source = config["wrap-git"]

        pkg = dep_packages.get(identifier)
        if pkg is not None:
            current_revision = pkg.version
        else:
            other_repo = ROOT_DIR / "subprojects" / identifier
            if other_repo.exists():
                current_revision = run(["git", "rev-parse", "HEAD"], cwd=other_repo).stdout.strip()
            else:
                url = source["url"]
                assert url.startswith("https://github.com/"), f"{url}: unhandled repo URL"
                assert url.endswith(".git")
                tokens = url[19:-4].split("/")
                assert len(tokens) == 2
                current_revision = query_repo_commits(organization=tokens[0], repo=tokens[1])["sha"]

        if source["revision"] != current_revision:
            source["revision"] = current_revision
            with wrapfile.open("w") as f:
                config.write(f)
            bumped_files.append(wrapfile)

    if bumped_files:
        run(["git", "add", *bumped_files], cwd=repo)
        run(["git", "commit", "-m", "subprojects: Bump outdated"], cwd=repo)
        print(f"\tsubprojects: bumped {', '.join([f.stem for f in bumped_files])}")
    else:
        print("\tsubprojects: up-to-date")

    push_changes(name, repo)


def bump_releng(releng: Path):
    if not (releng / "meson" / "meson.py").exists():
        run(["git", "submodule", "update", "--init", "--depth", "1", "--recursive", "releng"], cwd=releng.parent)
    run(["git", "checkout", "main"], cwd=releng)
    run(["git", "pull"], cwd=releng)


def bump_submodules() -> list[str]:
    print("# submodules")
    changes = query_local_changes(ROOT_DIR)
    relevant_changes = [relpath for kind, relpath in changes
                        if kind == "M" and (relpath == "releng" or relpath.startswith("subprojects/"))]
    assert len(changes) == len(relevant_changes), "frida: expected clean repo"
    if relevant_changes:
        run(["git", "add", *relevant_changes], cwd=ROOT_DIR)
        run(["git", "commit", "-m", "submodules: Bump outdated"], cwd=ROOT_DIR)
        print(f"\tbumped {', '.join([Path(relpath).name for relpath in relevant_changes])}")
    else:
        print("\tup-to-date")
    return relevant_changes


def tag(version: str):
    for _, repo in enumerate_projects_in_release_cycle():
        assert_no_local_changes(repo)
    for name, repo in enumerate_projects_in_release_cycle():
        prepublish(name, version, repo)

    bump_submodules()

    prepublish("frida", version, ROOT_DIR)


def prepublish(name: str, version: str, repo: Path):
    print("Prepublishing:", name)

    modified_wrapfiles: list[Path] = []
    for identifier, config, wrapfile in enumerate_git_wraps_in_repo(repo):
        if identifier in PROJECT_NAMES_IN_RELEASE_CYCLE:
            config["wrap-git"]["revision"] = version
            with wrapfile.open("w") as f:
                config.write(f)
            modified_wrapfiles.append(wrapfile)

    if modified_wrapfiles:
        run(["git", "add", *modified_wrapfiles], cwd=repo)
        run(["git", "commit", "-m", "subprojects: Prepare for release"], cwd=repo)
        print(f"\tsubprojects: prepared {', '.join([f.stem for f in modified_wrapfiles])}")
    else:
        print("\tsubprojects: no changes needed")

    run(["git", "tag", version], cwd=repo)
    run(["git", "push", "--atomic", "origin", "main", version], cwd=repo)
    print("\tpushed")


def backtag(version: str):
    run(["git", "checkout", version], cwd=ROOT_DIR)
    run(["git", "submodule", "update"], cwd=ROOT_DIR)
    for name, repo in enumerate_projects_in_release_cycle():
        if not run(["git", "tag", "-l", version], cwd=repo).stdout.strip():
            run(["git", "tag", version], cwd=repo)
            ensure_remote_origin_writable(name, repo)
            run(["git", "push", "origin", version], cwd=repo)


def enumerate_projects_in_release_cycle() -> Iterator[tuple[str, Path]]:
    for name in PROJECT_NAMES_IN_RELEASE_CYCLE:
        yield name, ROOT_DIR / "subprojects" / name


def enumerate_git_wraps_in_repo(repo: Path) -> Iterator[tuple[str, ConfigParser, Path]]:
    for wrapfile in (repo / "subprojects").glob("*.wrap"):
        identifier = wrapfile.stem

        config = ConfigParser()
        config.read(wrapfile)

        if "wrap-git" not in config:
            continue

        yield identifier, config, wrapfile


def assert_no_local_changes(repo: Path):
    assert not query_local_changes(repo), f"{repo.name}: expected clean repo"


def query_local_changes(repo: Path) -> list[str]:
    output = run(["git", "status", "--porcelain=v1"], cwd=repo).stdout.strip()
    if not output:
        return []
    return [tuple(line.strip().split(" ", maxsplit=1)) for line in output.split("\n")]


def push_changes(name: str, repo: Path):
    ensure_remote_origin_writable(name, repo)
    run(["git", "push", "-u", "origin", "main"], cwd=repo)


def ensure_remote_origin_writable(name: str, repo: Path):
    if "https:" in run(["git", "remote", "show", "origin", "-n"], cwd=repo).stdout:
        run(["git", "remote", "rm", "origin"], cwd=repo)
        run(["git", "remote", "add", "origin", f"git@github.com:frida/{name}.git"], cwd=repo)
        run(["git", "fetch", "origin"], cwd=repo)


def run(argv: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(argv,
                          capture_output=True,
                          encoding="utf-8",
                          check=True,
                          **kwargs)


if __name__ == "__main__":
    main(sys.argv)
