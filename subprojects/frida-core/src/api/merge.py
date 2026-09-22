from pathlib import Path
import shutil
import subprocess
import sys


def main(argv):
    lib_binary, libtool_binary, ar_binary = [Path(p) if p else None for p in argv[1:4]]
    build_dir = Path(argv[4])
    output_lib = Path(argv[5])
    input_libs = [Path(p) for p in argv[6:]]

    if lib_binary is not None:
        subprocess.run([lib_binary, "/nologo", f"/out:{output_lib}"] + input_libs,
                       check=True)
    elif libtool_binary is not None:
        subprocess.run([libtool_binary, "-static", "-o", output_lib] + input_libs,
                       check=True)
    else:
        mri_lines = [f"create {output_lib}"]
        for lib in input_libs:
            command = "addlib" if lib.suffix == ".a" else "addmod"
            mri_lines += [f"{command} {lib}"]
        mri_lines += ["save", "end"]

        subprocess.run([ar_binary, "-M"],
                       input="\n".join(mri_lines),
                       encoding="utf-8",
                       check=True)


if __name__ == "__main__":
    main(sys.argv)
