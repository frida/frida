import os
import re
import sys
from io import StringIO
from pathlib import Path


def main(argv):
    (
        output_dir,
        input_dir,
        gum_dir,
        capstone_incdir,
        libtcc_incdir,
    ) = [Path(d).resolve() if d else None for d in argv[1:6]]
    arch = argv[6]

    try:
        generate_runtime(
            output_dir,
            "gumcmodule-runtime.h",
            input_dir,
            gum_dir,
            capstone_incdir,
            libtcc_incdir,
            arch,
        )
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


cmodule_function_pattern = re.compile(
    r"^(void|size_t|int|unsigned int|bool|const char \*|gpointer|gsize|gssize|gint[0-9]*|guint[0-9]*|gfloat|gdouble|gboolean||(?:const )?\w+ \*|Gum\w+|csh|cs_err) ([a-z][a-z0-9_]+)\s?\(",
    re.MULTILINE,
)
cmodule_variable_pattern = re.compile(r"^(extern .+? )(\w+);", re.MULTILINE)
capstone_include_pattern = re.compile(r'^#include "(\w+)\.h"$', re.MULTILINE)
capstone_export_pattern = re.compile(r"^CAPSTONE_EXPORT$", re.MULTILINE)

c_comment_pattern = re.compile(r"\/\*(\*(?!\/)|[^*])*\*\/")
cpp_comment_pattern = re.compile(r"\s+?\/\/.+")


def generate_runtime(
    output_dir, output, input_dir, gum_dir, capstone_incdir, libtcc_incdir, arch
):
    if arch.startswith("x86") or arch == "x64":
        writer_arch = "x86"
    elif arch.startswith("mips"):
        writer_arch = "mips"
    else:
        writer_arch = arch
    capstone_arch = writer_arch

    def gum_header_matches_writer(name):
        if writer_arch == "arm":
            return name in ("gumarmwriter.h", "gumthumbwriter.h")
        else:
            return name == "gum" + writer_arch + "writer.h"

    def optimize_gum_header(source):
        return source.replace("GUM_API ", "")

    def capstone_header_matches_arch(name):
        if name in ("capstone.h", "platform.h"):
            return True
        return name == capstone_arch + ".h"

    def optimize_capstone_header(source):
        result = capstone_include_pattern.sub(transform_capstone_include, source)
        result = capstone_export_pattern.sub("", result)
        result = result.replace("CAPSTONE_API ", "")
        return result

    def transform_capstone_include(m):
        name = m.group(1)

        if name in ("platform", capstone_arch):
            return m.group(0)

        if name == "systemz":
            name = "sysz"

        return "typedef int cs_{0};".format(name)

    def libtcc_is_header(name):
        """Ignore symbols from the TinyCC standard library: dlclose() etc."""
        return is_header(name) and name != "tcclib.h"

    inputs = [
        (
            input_dir / "runtime" / "cmodule",
            None,
            is_header,
            identity_transform,
            "GUM_CHEADER_FRIDA",
        ),
        (
            gum_dir / ("arch-" + writer_arch),
            gum_dir.parent,
            gum_header_matches_writer,
            optimize_gum_header,
            "GUM_CHEADER_FRIDA",
        ),
        (
            capstone_incdir,
            None,
            capstone_header_matches_arch,
            optimize_capstone_header,
            "GUM_CHEADER_FRIDA",
        ),
    ]
    if libtcc_incdir is not None:
        inputs += [
            (
                input_dir / "runtime" / "cmodule-tcc",
                None,
                is_header,
                identity_transform,
                "GUM_CHEADER_TCC",
            ),
            (
                libtcc_incdir,
                None,
                libtcc_is_header,
                identity_transform,
                "GUM_CHEADER_TCC",
            ),
        ]

    with OutputFile(output_dir / output) as output_file:
        modules = []
        symbols = []

        for (
            header_dir,
            header_reldir,
            header_filter,
            header_transform,
            header_kind,
        ) in inputs:
            for header_name, header_source in find_headers(
                header_dir, header_reldir, header_filter, header_transform
            ):
                input_identifier = "gum_cmodule_{0}".format(identifier(header_name))

                for pattern in (cmodule_function_pattern, cmodule_variable_pattern):
                    for m in pattern.finditer(header_source):
                        name = m.group(2)
                        if name.startswith("cs_arch_register_"):
                            continue
                        symbols.append(name)

                source_bytes = bytearray(header_source.encode("utf-8"))
                source_bytes.append(0)
                source_size = len(source_bytes)

                output_file.write(
                    "static const gchar {0}[{1}] =\n{{".format(
                        input_identifier, source_size
                    )
                )
                write_bytes(source_bytes, output_file, "signed")
                output_file.write("\n};\n\n")

                modules.append(
                    (header_name, input_identifier, source_size - 1, header_kind)
                )

        output_file.write("static const GumCHeaderDetails gum_cmodule_headers[] =\n{")
        for input_name, input_identifier, input_size, header_kind in modules:
            output_file.write(
                '\n  {{ "{0}", {1}, {2}, {3} }},'.format(
                    input_name, input_identifier, input_size, header_kind
                )
            )
        output_file.write("\n};\n")

        symbol_insertions = [
            '    g_hash_table_insert (symbols, "{0}", GUM_FUNCPTR_TO_POINTER ({0}));'.format(
                name
            )
            for name in symbols
        ]
        output_file.write(
            """
static void gum_cmodule_deinit_symbols (void);

static GHashTable *
gum_cmodule_get_symbols (void)
{{
  static gsize gonce_value;

  if (g_once_init_enter (&gonce_value))
  {{
    GHashTable * symbols;

    symbols = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4996)
#endif

{insertions}

#ifdef _MSC_VER
# pragma warning (pop)
#endif

    _gum_register_destructor (gum_cmodule_deinit_symbols);

    g_once_init_leave (&gonce_value, GPOINTER_TO_SIZE (symbols) + 1);
  }}

  return GSIZE_TO_POINTER (gonce_value - 1);
}}

static void
gum_cmodule_deinit_symbols (void)
{{
  g_hash_table_unref (gum_cmodule_get_symbols ());
}}
""".format(
                insertions="\n".join(symbol_insertions)
            )
        )


def find_headers(include_dir, relative_to_dir, is_header, transform):
    if relative_to_dir is None:
        relative_to_dir = include_dir

    for root, dirs, files in os.walk(include_dir):
        for name in files:
            if is_header(name):
                path = Path(root) / name
                name = os.path.relpath(path, relative_to_dir).replace("\\", "/")
                source = strip_header(
                    transform(strip_header(path.read_text(encoding="utf-8")))
                )
                yield (name, source)


def is_header(name):
    return name.endswith(".h")


def identity_transform(v):
    return v


def strip_header(source):
    result = c_comment_pattern.sub("", source)
    result = cpp_comment_pattern.sub("", result)
    while True:
        if "\n\n" not in result:
            break
        result = result.replace("\n\n", "\n")
    return result


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
