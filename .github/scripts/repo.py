#!/usr/bin/env python3

import argparse
from configparser import ConfigParser
from pathlib import Path
import subprocess
import sys
from typing import Iterator

ROOT_DIR = Path(__file__).parent.resolve()
RELENG_DIR = ROOT_DIR / "releng"
sys.path.insert(0, str(ROOT_DIR))
from releng.deps import load_dependency_parameters


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
    for name, repo in projects_in_release_cycle():
        bump_subproject(name, repo)
    bump_submodules()


def bump_subproject(name: str, repo: Path):
    assert_no_local_changes(name, repo)

    print("Bumping:", name)
    run(["git", "checkout", "main"], cwd=repo)
    run(["git", "pull"], cwd=repo)

    repo_releng = repo / "releng"
    if not (repo_releng / "meson" / "meson.py").exists():
        run(["git", "submodule", "update", "--init", "--depth", "1", "--recursive", "releng"], cwd=repo)
    run(["git", "checkout", "main"], cwd=repo_releng)
    run(["git", "pull"], cwd=repo_releng)
    if query_local_changes(repo):
        run(["git", "submodule", "update"], cwd=repo_releng)
        run(["git", "add", "releng"], cwd=repo)
        run(["git", "commit", "-m", "submodules: Bump releng"], cwd=repo)

    dep_packages = load_dependency_parameters().packages
    bumped_files: list[Path] = []
    for wrapfile in (repo / "subprojects").glob("*.wrap"):
        config = ConfigParser()
        config.read(wrapfile)

        pkg_id = wrapfile.stem
        pkg = dep_packages.get(pkg_id)
        if pkg is not None:
            current_revision = pkg.version
        else:
            other_repo = ROOT_DIR / "subprojects" / pkg_id
            assert other_repo.exists(), f"{pkg_id}: unknown subproject"
            current_revision = run(["git", "rev-parse", "HEAD"], cwd=other_repo).stdout.strip()

        if config["wrap-git"]["revision"] != current_revision:
            config["wrap-git"]["revision"] = current_revision
            with wrapfile.open("w") as f:
                config.write(f)
            bumped_files.append(wrapfile)

    if bumped_files:
        run(["git", "add", *bumped_files], cwd=repo)
        run(["git", "commit", "-m", "subprojects: Bump outdated"], cwd=repo)

    push_changes(name, repo)


def bump_submodules():
    changes = query_local_changes(ROOT_DIR)
    relevant_changes = [name for kind, name in changes
                        if kind == "M" and (name.startswith("releng") or name.startswith("subprojects/"))]
    assert len(changes) == len(relevant_changes), "frida: expected clean repo"
    if relevant_changes:
        run(["git", "add", *relevant_changes], cwd=ROOT_DIR)
        run(["git", "commit", "-m", "submodules: Bump outdated"], cwd=ROOT_DIR)
        push_changes("frida", ROOT_DIR)


def tag(version: str):
    for name, repo in projects_in_release_cycle():
        tag_repo(name, version, repo)
    tag_repo("frida", version, ROOT_DIR)


def tag_repo(name: str, version: str, repo: Path):
    assert_no_local_changes(name, repo)
    run(["git", "tag", version], cwd=repo)
    run(["git", "push", "--atomic", "origin", "main", version], cwd=repo)


def backtag(version: str):
    run(["git", "checkout", version], cwd=ROOT_DIR)
    run(["git", "submodule", "update"], cwd=ROOT_DIR)
    for name, repo in projects_in_release_cycle():
        if not run(["git", "tag", "-l", version], cwd=repo).stdout.strip():
            run(["git", "tag", version], cwd=repo)
            ensure_remote_origin_writable(name, repo)
            run(["git", "push", "origin", version], cwd=repo)


def projects_in_release_cycle() -> Iterator[tuple[str, Path]]:
    for name in PROJECT_NAMES_IN_RELEASE_CYCLE:
        yield name, ROOT_DIR / "subprojects" / name


def assert_no_local_changes(name: str, repo: Path):
    assert not query_local_changes(repo), f"{name}: expected clean repo"


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
