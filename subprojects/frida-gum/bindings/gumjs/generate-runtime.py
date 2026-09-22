import subprocess
import sys
from dataclasses import dataclass
from io import StringIO
from pathlib import Path
from typing import List, Optional


@dataclass
class JSSource:
    name: str
    path: Path
    component: Optional[str]


def main(argv):
    (
        output_dir,
        priv_dir,
        input_dir,
        quickcompile,
    ) = [Path(d).resolve() if d else None for d in argv[1:5]]
    backends = set(argv[5].split(","))
    endian = argv[6]
    sources = [Path(d).resolve() for d in argv[7:]]

    try:
        generate_runtime(
            sources,
            output_dir,
            priv_dir,
            input_dir,
            quickcompile,
            backends,
            endian,
        )
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def generate_runtime(
    sources: List[str],
    output_dir: Path,
    priv_dir: Path,
    input_dir: Path,
    quickcompile: Path,
    backends: List[str],
    endian: str,
):
    js_sources = []
    for source in sources:
        if source.suffix != ".js":
            continue

        name = "/".join(source.relative_to(input_dir / "runtime").parts)

        last_part = source.stem.split("-")[-1]
        component = last_part if last_part in {"quickjs", "v8"} else None

        js_sources.append(JSSource(name, source, component))

    if "qjs" in backends:
        qcflags = []
        if endian != sys.byteorder:
            qcflags.append("--bswap")

        generate_runtime_quick(
            output_dir,
            priv_dir,
            input_dir,
            [s for s in js_sources if s.component is None or s.component == "quickjs"],
            quickcompile,
            qcflags,
        )

    if "v8" in backends:
        generate_runtime_v8(
            output_dir,
            priv_dir,
            [s for s in js_sources if s.component is None or s.component == "v8"],
        )

    (output_dir / "runtime.bundle").write_bytes(b"")


def generate_runtime_quick(
    output_dir: Path,
    priv_dir: Path,
    input_dir: Path,
    sources: List[JSSource],
    quickcompile: Path,
    flags: List[str],
):
    with OutputFile(output_dir / "gumquickscript-runtime.h") as output_file:
        output_file.write(
            """\
typedef struct _GumQuickRuntimeModule GumQuickRuntimeModule;

struct _GumQuickRuntimeModule
{
  const gchar * name;
  gconstpointer bytecode;
  gsize bytecode_size;
};
"""
        )

        subprocess.run(
            [quickcompile]
            + flags
            + [priv_dir, input_dir]
            + [s.path.relative_to(input_dir) for s in sources],
            check=True,
        )

        modules = []
        for source in sources:
            stem = source.path.stem
            dest_path = priv_dir / (stem + ".qjs")

            bytecode = dest_path.read_bytes()
            bytecode_size = len(bytecode)

            stem_cname = identifier(stem)
            input_bytecode_identifier = "gumjs_{0}_bytecode".format(stem_cname)

            output_file.write(
                "\nstatic const guint8 {0}[{1}] =\n{{".format(
                    input_bytecode_identifier, bytecode_size
                )
            )
            write_bytes(bytecode, output_file, "unsigned")
            output_file.write("\n};\n")

            modules.append((source.name, input_bytecode_identifier, bytecode_size))

        output_file.write(
            "\nstatic const GumQuickRuntimeModule gumjs_runtime_modules[] =\n{"
        )
        for name, bytecode_identifier, bytecode_size in modules:
            output_file.write(
                f'\n  {{ "{name}", {bytecode_identifier}, {bytecode_size} }},'
            )
        output_file.write("\n  { NULL, NULL, 0 }\n};")


def generate_runtime_v8(output_dir: Path, priv_dir: Path, sources: List[JSSource]):
    with OutputFile(output_dir / "gumv8script-runtime.h") as output_file:
        output_file.write(
            """\
struct GumV8RuntimeModule
{
  const gchar * name;
  const gchar * source_code;
};
"""
        )

        modules = []
        for source in sources:
            stem_cname = identifier(source.path.stem)
            input_source_code_identifier = f"gumjs_{stem_cname}_source_code"

            source_code = source.path.read_text(encoding="utf-8")
            source_code_bytes = bytearray(source_code.encode("utf-8"))
            source_code_bytes.append(0)
            source_code_size = len(source_code_bytes)

            output_file.write(
                f"\nstatic const gchar {input_source_code_identifier}[{source_code_size}] =\n{{"
            )
            write_bytes(source_code_bytes, output_file, "signed")
            output_file.write("\n};\n")

            modules.append((source.path.name, input_source_code_identifier))

        output_file.write(
            "\nstatic const GumV8RuntimeModule gumjs_runtime_modules[] =\n{"
        )
        for filename, source_code_identifier in modules:
            output_file.write(f'\n  {{ "{filename}", {source_code_identifier}, }},')
        output_file.write("\n  { NULL, NULL }\n};")

def write_bytes(data, sink, encoding):
    sink.write("\n  ")
    line_length = 0
    offset = 0
    for b in bytearray(data):
        if offset > 0:
            sink.write(",")
            line_length += 1
        if line_length >= 70:
            sink.write("\n  ")
            line_length = 0
        if encoding == "signed" and b >= 128:
            b -= 256
        token = str(b)
        sink.write(token)

        line_length += len(token)
        offset += 1


def identifier(filename):
    result = ""
    if filename.startswith("frida-"):
        filename = filename[6:]
    for c in filename:
        if c.isalnum():
            result += c.lower()
        else:
            result += "_"
    return result


class OutputFile:
    def __init__(self, output_path):
        self._output_path = output_path
        self._io = StringIO()

    def __enter__(self):
        return self._io

    def __exit__(self, *exc):
        result = self._io.getvalue()
        if self._output_path.exists():
            existing_contents = self._output_path.read_text(encoding="utf-8")
            if existing_contents == result:
                return False
        self._output_path.write_text(result, encoding="utf-8")
        return False


if __name__ == "__main__":
    main(sys.argv)
