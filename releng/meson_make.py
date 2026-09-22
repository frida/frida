import argparse
import os
from pathlib import Path
import pickle
import shlex
import shutil
import sys
from typing import Callable, Dict, List

from . import env
from .meson_configure import configure


STANDARD_TARGET_NAMES = ["all", "clean", "distclean", "install", "test"]


def main():
    default_sourcedir = Path(sys.argv.pop(1)).resolve()
    sourcedir = Path(os.environ.get("MESON_SOURCE_ROOT", default_sourcedir)).resolve()

    default_builddir = Path(sys.argv.pop(1)).resolve()
    builddir = Path(os.environ.get("MESON_BUILD_ROOT", default_builddir)).resolve()

    parser = argparse.ArgumentParser(prog="make")
    parser.add_argument("targets",
                        help="Targets to build, e.g.: " + ", ".join(STANDARD_TARGET_NAMES),
                        nargs="*",
                        default="all")
    options = parser.parse_args()

    targets = options.targets
    if isinstance(targets, str):
        targets = [targets]

    try:
        make(sourcedir, builddir, targets)
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def make(sourcedir: Path,
         builddir: Path,
         targets: List[str],
         environ: Dict[str, str] = os.environ,
         call_meson: Callable = env.call_meson):
    if not (builddir / "build.ninja").exists():
        configure(sourcedir, builddir, environ=environ)

    compile_options = []
    if environ.get("V") == "1":
        compile_options += ["-v"]

    test_options = shlex.split(environ.get("FRIDA_TEST_OPTIONS", "-v"))

    env_state = pickle.loads((builddir / "frida-env.dat").read_bytes())

    install_options = []
    if env_state.get("default_library") == "shared":
        install_options += ["--skip-subprojects"]

    standard_targets = {
        "all": ["compile"] + compile_options,
        "clean": ["compile", "--clean"] + compile_options,
        "distclean": lambda: distclean(sourcedir, builddir),
        "install": ["install"] + install_options,
        "test": ["test"] + test_options,
    }

    machine_config = env_state["host"]
    if machine_config is None:
        machine_config = env_state["build"]
    meson_env = machine_config.make_merged_environment(environ)
    meson_env["FRIDA_ALLOWED_PREBUILDS"] = ",".join(env_state["allowed_prebuilds"])
    meson_env["FRIDA_DEPS"] = str(env_state["deps"])

    def do_meson_command(args):
        call_meson(args,
                   use_submodule=env_state["meson"] == "internal",
                   cwd=builddir,
                   env=meson_env,
                   check=True)

    pending_targets = targets.copy()
    pending_compile = None

    while pending_targets:
        target = pending_targets.pop(0)

        action = standard_targets.get(target, None)
        if action is None:
            meson_command = "compile"
        elif not callable(action):
            meson_command = action[0]
        else:
            meson_command = None

        if meson_command == "compile":
            if pending_compile is None:
                pending_compile = ["compile"]
            if action is not None:
                pending_compile += action[1:]
            else:
                pending_compile += [target]
            continue

        if pending_compile is not None:
            do_meson_command(pending_compile)
            pending_compile = None

        if meson_command is not None:
            do_meson_command(action)
        else:
            action()

    if pending_compile is not None:
        do_meson_command(pending_compile)


def distclean(sourcedir: Path, builddir: Path):
    items_to_delete = []

    if not builddir.is_relative_to(sourcedir):
        items_to_delete += list(builddir.iterdir())

    items_to_delete += [
        sourcedir / "build",
        sourcedir / "deps",
    ]

    for item in items_to_delete:
        try:
            shutil.rmtree(item)
        except:
            pass
