import os
from pathlib import Path
import shutil
import subprocess
import sys


def main(argv):
    args = argv[1:]
    host_os = args.pop(0)
    host_abi = args.pop(0)
    strip_command = pop_cmd_array_arg(args)
    strip_enabled = args.pop(0) == "true"
    install_name_tool = pop_cmd_array_arg(args)
    codesign = pop_cmd_array_arg(args)
    termux_elf_cleaner = pop_cmd_array_arg(args)
    output_path = Path(args.pop(0))
    input_path = Path(args.pop(0))
    kind = args.pop(0)
    assert kind in {"executable", "shared-library"}
    identity = args.pop(0)
    if kind == "executable":
        input_entitlements_path = args.pop(0) if args else None
    else:
        input_entitlements_path = None
    flags = set(args.pop(0).split("|") if args else [])

    is_apple_os = host_os in {"macos", "ios", "watchos", "tvos"}

    if is_apple_os:
        envvar_name = f"{host_os.upper()}_CERTID"
        certid = os.environ.get(envvar_name, None)
        if certid is None:
            print(f"{envvar_name} not set, see https://github.com/frida/frida#apple-oses",
                  file=sys.stderr)
            sys.exit(1)
    else:
        certid = None

    intermediate_path = output_path.parent / f"{output_path.name}.tmp"
    shutil.copy(input_path, intermediate_path)

    try:
        run_kwargs = {
            "stdout": subprocess.PIPE,
            "stderr": subprocess.STDOUT,
            "encoding": "utf-8",
            "check": True,
        }

        if strip_enabled and strip_command is not None:
            subprocess.run(strip_command + [intermediate_path], **run_kwargs)

        if is_apple_os:
            if kind == "shared-library":
                subprocess.run(install_name_tool + ["-id", identity, intermediate_path], **run_kwargs)

            codesign_args = ["-f", "-s", certid]
            if kind == "executable":
                if host_os == "macos":
                    codesign_args += ["-i", identity]
                if input_entitlements_path is not None and host_os in {"ios", "tvos"}:
                    codesign_args += ["--entitlements", input_entitlements_path]
            subprocess.run(codesign + codesign_args + [intermediate_path], **run_kwargs)

        if host_os == "android" and "elf-cleaner:off" not in flags:
            api_level = 19 if host_abi in {"x86", "arm"} else 21
            subprocess.run(termux_elf_cleaner + ["--api-level", str(api_level), "--quiet", intermediate_path],
                           **run_kwargs)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(1)

    shutil.move(intermediate_path, output_path)


def pop_cmd_array_arg(args):
    result = []
    first = args.pop(0)
    assert first == ">>>"
    while True:
        cur = args.pop(0)
        if cur == "<<<":
            break
        result.append(cur)
    if len(result) == 1 and not result[0]:
        return None
    return result


if __name__ == "__main__":
    main(sys.argv)
