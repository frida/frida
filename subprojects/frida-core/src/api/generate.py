from __future__ import annotations
import argparse
from dataclasses import dataclass
from io import StringIO
from pathlib import Path
import re
from typing import List, Set
import xml.etree.ElementTree as ET

CORE_NAMESPACE = "http://www.gtk.org/introspection/core/1.0"
C_NAMESPACE = "http://www.gtk.org/introspection/c/1.0"
GLIB_NAMESPACE = "http://www.gtk.org/introspection/glib/1.0"
GIR_NAMESPACES = {
    "": CORE_NAMESPACE,
    "c": C_NAMESPACE,
    "glib": GLIB_NAMESPACE,
}

CORE_TAG_IMPLEMENTS = f"{{{CORE_NAMESPACE}}}implements"
CORE_TAG_FIELD = f"{{{CORE_NAMESPACE}}}field"
CORE_TAG_CONSTRUCTOR = f"{{{CORE_NAMESPACE}}}constructor"
CORE_TAG_METHOD = f"{{{CORE_NAMESPACE}}}method"

INTERNAL_INTERFACES = {
    "Frida.HostSessionHub",
    "Frida.AgentMessageSink",
    "FridaBase.AgentMessageSink",
    "Json.Serializable",
}

OBJECT_TYPE_PATTERN = re.compile(r"\bpublic\s+(sealed |abstract )?(class|interface)\s+(\w+)\b")

TOPLEVEL_NAMES = [
    "frida.vala",
    "package-manager.vala",
    "control-service.vala",
    "portal-service.vala",
    "file-monitor.vala",
    Path("compiler") / "compiler.vala",
]

def main():
    parser = argparse.ArgumentParser(description="Generate refined Frida API definitions")
    parser.add_argument('--output', dest='output_type', choices=['bundle', 'header', 'gir', 'vapi', 'vapi-stamp', 'symbol-maps'], default='bundle')
    parser.add_argument('output_dir', metavar='/output/dir')
    parser.add_argument('extra_args', nargs='*')

    args = parser.parse_args()

    output_type = args.output_type
    output_dir = Path(args.output_dir)

    if output_type == 'symbol-maps':
        emit_symbol_maps_from_source(output_dir)
        return

    extra = args.extra_args
    frida_version = extra[0]
    frida_version_components = tuple(extra[1:5])
    api_version = extra[5]

    if output_type == 'vapi-stamp':
        (output_dir / f"frida-core-{api_version}.vapi.stamp").write_bytes(b"")
        return

    core_header = Path(extra[6]).read_text(encoding='utf-8')
    core_gir = Path(extra[7]).read_text(encoding='utf-8')
    core_vapi = Path(extra[8]).read_text(encoding='utf-8')
    base_header = Path(extra[9]).read_text(encoding='utf-8')
    base_gir = Path(extra[10]).read_text(encoding='utf-8')
    base_vapi = Path(extra[11]).read_text(encoding='utf-8')

    toplevel_sources = []
    src_dir = Path(__file__).parent.parent.resolve()
    for name in TOPLEVEL_NAMES:
        toplevel_sources.append((src_dir / name).read_text(encoding='utf-8'))
    toplevel_code = "\n".join(toplevel_sources)

    enable_header = False
    enable_gir = False
    enable_vapi = False
    if output_type == 'bundle':
        enable_header = True
        enable_gir = True
        enable_vapi = True
    elif output_type == 'header':
        enable_header = True
    elif output_type == 'gir':
        enable_gir = True
    elif output_type == 'vapi':
        enable_vapi = True

    api = parse_api(frida_version, frida_version_components, api_version, toplevel_code, core_header, core_vapi, base_header, base_vapi)

    if enable_header:
        emit_header(api, output_dir)

    if enable_gir:
        emit_gir(api, core_gir, base_gir, output_dir, build_doc_index(src_dir))

    if enable_vapi:
        emit_vapi(api, output_dir)

def emit_header(api, output_dir):
    with OutputFile(output_dir / 'frida-core.h') as output_header_file:
        output_header_file.write("#ifndef __FRIDA_CORE_H__\n#define __FRIDA_CORE_H__\n\n")

        output_header_file.write("#include <glib.h>\n#include <glib-object.h>\n#include <gio/gio.h>\n#include <json-glib/json-glib.h>\n\n")

        output_header_file.write(f"#define FRIDA_VERSION \"{api.frida_version}\"\n\n")

        for name, value in zip(['MAJOR', 'MINOR', 'MICRO', 'NANO'], api.frida_version_components):
            output_header_file.write(f"#define FRIDA_{name}_VERSION {value}\n")

        output_header_file.write("""
#define FRIDA_CHECK_VERSION(maj, min, mic) \\
    (FRIDA_CURRENT_VERSION >= FRIDA_VERSION_ENCODE ((maj), (min), (mic)))

#define FRIDA_CURRENT_VERSION \\
    FRIDA_VERSION_ENCODE (    \\
        FRIDA_MAJOR_VERSION,  \\
        FRIDA_MINOR_VERSION,  \\
        FRIDA_MICRO_VERSION)

#define FRIDA_VERSION_ENCODE(maj, min, mic) \\
    (((maj) * 1000000U) + ((min) * 1000U) + (mic))
""")

        output_header_file.write("\nG_BEGIN_DECLS\n")

        for object_type in api.object_types:
            output_header_file.write("\ntypedef struct _%s %s;" % (object_type.c_name, object_type.c_name))
            if object_type.c_iface_definition is not None:
                output_header_file.write("\ntypedef struct _%sIface %sIface;" % (object_type.c_name, object_type.c_name))

        for enum in api.enum_types:
            output_header_file.write("\n\n" + enum.c_definition)

        output_header_file.write("\n\n/* Library lifetime */")
        output_header_file.write("\nvoid frida_init (void);")
        output_header_file.write("\nvoid frida_init_with_runtime (FridaRuntime runtime);")
        output_header_file.write("\nvoid frida_shutdown (void);")
        output_header_file.write("\nvoid frida_deinit (void);")
        output_header_file.write("\nGMainContext * frida_get_main_context (void);")

        output_header_file.write("\n\n/* Object lifetime */")
        output_header_file.write("\nvoid frida_unref (gpointer obj);")

        output_header_file.write("\n\n/* Library versioning */")
        output_header_file.write("\nvoid frida_version (guint * major, guint * minor, guint * micro, guint * nano);")
        output_header_file.write("\nconst gchar * frida_version_string (void);")

        for object_type in api.object_types:
            output_header_file.write("\n\n/* %s */" % object_type.name)
            sections = []
            if len(object_type.c_delegate_typedefs) > 0:
                sections.append("\n" + "\n".join(object_type.c_delegate_typedefs))
            if object_type.c_iface_definition is not None:
                sections.append("\n" + object_type.c_iface_definition)
            if len(object_type.c_constructors) > 0:
                sections.append("\n" + "\n".join(object_type.c_constructors))
            if len(object_type.c_getter_prototypes) > 0:
                sections.append("\n" + "\n".join(object_type.c_getter_prototypes))
            if len(object_type.c_method_prototypes) > 0:
                sections.append("\n" + "\n".join(object_type.c_method_prototypes))
            output_header_file.write("\n".join(sections))

        output_header_file.write("\n\n/* Toplevel functions */")
        for func in api.functions:
            output_header_file.write("\n" + func.c_prototype)

        if len(api.error_types) > 0:
            output_header_file.write("\n\n/* Errors */\n")
            output_header_file.write("\n\n".join(map(lambda enum: "GQuark frida_%(name_lc)s_quark (void);\n" \
                % { 'name_lc': enum.name_lc }, api.error_types)))
            output_header_file.write("\n")
            output_header_file.write("\n\n".join(map(lambda enum: enum.c_definition, api.error_types)))

        output_header_file.write("\n\n/* GTypes */")
        for enum in api.enum_types:
            output_header_file.write("\nGType %s_get_type (void) G_GNUC_CONST;" % enum.c_name_lc)
        for object_type in api.object_types:
            if object_type.c_get_type is not None:
                output_header_file.write("\n" + object_type.c_get_type)

        output_header_file.write("\n\n/* Macros */")
        macros = []
        for enum in api.enum_types:
            macros.append("#define FRIDA_TYPE_%(name_uc)s (frida_%(name_lc)s_get_type ())" \
                % { 'name_lc': enum.name_lc, 'name_uc': enum.name_uc })
        for object_type in api.object_types:
            macros.append("""#define FRIDA_TYPE_%(name_uc)s (frida_%(name_lc)s_get_type ())
#define FRIDA_%(name_uc)s(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRIDA_TYPE_%(name_uc)s, Frida%(name)s))
#define FRIDA_IS_%(name_uc)s(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRIDA_TYPE_%(name_uc)s))""" \
                % { 'name': object_type.name, 'name_lc': object_type.name_lc, 'name_uc': object_type.name_uc })

        for enum in api.error_types:
            macros.append("#define FRIDA_%(name_uc)s (frida_%(name_lc)s_quark ())" \
                % { 'name_lc': enum.name_lc, 'name_uc': enum.name_uc })
        output_header_file.write("\n" + "\n\n".join(macros))

        output_header_file.write("\n\nG_END_DECLS")

        output_header_file.write("\n\n#endif\n")

def emit_gir(api: ApiSpec, core_gir: str, base_gir: str, output_dir: Path, docs: DocIndex) -> str:
    ET.register_namespace("", CORE_NAMESPACE)
    ET.register_namespace("c", C_NAMESPACE)
    ET.register_namespace("glib", GLIB_NAMESPACE)

    core_tree = ET.ElementTree(ET.fromstring(core_gir))
    base_tree = ET.ElementTree(ET.fromstring(base_gir))

    core_root = core_tree.getroot()
    base_root = base_tree.getroot()

    merged_root = ET.Element(core_root.tag, core_root.attrib)

    for elem in core_root.findall("include", GIR_NAMESPACES):
        name = elem.get("name")
        if name in {"GLib", "GObject", "Gio"}:
            merged_root.append(elem)

    for tag in ["package", "c:include"]:
        for elem in core_root.findall(tag, GIR_NAMESPACES):
            merged_root.append(elem)

    core_namespace = core_root.find("namespace", GIR_NAMESPACES)
    merged_namespace = ET.SubElement(merged_root, core_namespace.tag, core_namespace.attrib)

    object_type_names = {obj.name for obj in api.object_types}
    enum_type_names = {enum.name for enum in api.enum_types}
    error_type_names = {error.name for error in api.error_types}

    def merge_and_transform_elements(tag_name: str, spec_set: Set[str]):
        core_elements = filter_elements(core_root.findall(f".//{tag_name}", GIR_NAMESPACES), spec_set)
        base_elements = filter_elements(base_root.findall(f".//{tag_name}", GIR_NAMESPACES), spec_set)
        for elem in core_elements + base_elements:
            if tag_name == "class":
                for child in list(elem):
                    if (child.tag == CORE_TAG_IMPLEMENTS and child.get("name") in INTERNAL_INTERFACES) \
                            or child.tag == CORE_TAG_FIELD \
                            or child.get("name").startswith("_") \
                            or (child.tag == CORE_TAG_CONSTRUCTOR and is_async_constructor(child)):
                        elem.remove(child)
            merged_namespace.append(elem)

    merge_and_transform_elements("class", object_type_names)
    merge_and_transform_elements("interface", object_type_names)
    merge_and_transform_elements("enumeration", enum_type_names | error_type_names)

    function_names = {func.name for func in api.functions}
    for source_root in [core_root, base_root]:
        source_namespace = source_root.find("namespace", GIR_NAMESPACES)
        for function in source_namespace.findall("function", GIR_NAMESPACES):
            if function.get("name") in function_names:
                merged_namespace.append(function)

    for source_root in [core_root, base_root]:
        for record in source_root.findall(".//record", GIR_NAMESPACES):
            owner = record.get(f"{{{GLIB_NAMESPACE}}}is-gtype-struct-for")
            if owner is not None and owner in object_type_names:
                merged_namespace.append(record)

    known_names = object_type_names | enum_type_names | error_type_names
    referenced_names = set()
    for elem in merged_namespace.iter():
        type_name = elem.get("name")
        if type_name is None:
            continue
        for prefix in ("Frida.", "FridaBase."):
            if type_name.startswith(prefix):
                referenced_names.add(type_name[len(prefix):])
                break
    needed_names = referenced_names - known_names
    for source_root in [core_root, base_root]:
        source_namespace = source_root.find("namespace", GIR_NAMESPACES)
        for callback in source_namespace.findall("callback", GIR_NAMESPACES):
            if callback.get("name") in needed_names:
                merged_namespace.append(callback)

    inject_documentation(merged_namespace, docs)

    ET.indent(merged_root, space="  ")
    result = ET.tostring(merged_root,
                         encoding="unicode",
                         xml_declaration=True)
    result = result.replace("FridaBase.", "Frida.")
    with OutputFile(output_dir / f"Frida-{api.version}.gir") as output_gir:
        output_gir.write(result)

def filter_elements(elements: List[ET.Element], spec_set: Set[str]):
    return [elem for elem in elements if elem.get("name") in spec_set]

def is_async_constructor(constructor: ET.Element) -> bool:
    for type_elem in constructor.iter(f"{{{CORE_NAMESPACE}}}type"):
        if type_elem.get("name") == "Gio.Cancellable":
            return True
    return False

@dataclass
class DocComment:
    body: str
    params: dict
    returns: str
    filename: str = "<generated>"
    line: int = 0

@dataclass
class DocIndex:
    types: dict          # type_name -> DocComment
    members: dict        # (type_name, member_name) -> DocComment
    enum_members: dict   # (enum_name, member_name) -> DocComment

DOC_TYPE_PATTERN = re.compile(r"public\s+(?:sealed\s+|abstract\s+)?(class|interface|enum|errordomain)\s+(\w+)")
DOC_SIGNAL_PATTERN = re.compile(r"public\s+signal\s+\S.*?\b(\w+)\s*\(")
DOC_PROPERTY_PATTERN = re.compile(r"public\s+(?:unowned\s+)?\S.*?\b(\w+)\s*\{")
DOC_METHOD_PATTERN = re.compile(r"public\s+\S.*?\b(\w+)\s*\(")
DOC_ENUM_MEMBER_PATTERN = re.compile(r"([A-Z][A-Z0-9_]*)\b")

def build_doc_index(src_dir: Path) -> DocIndex:
    repo_root = src_dir.parent
    sources = []
    for name in TOPLEVEL_NAMES:
        path = src_dir / name
        if path.exists():
            sources.append(path)
    base_dir = src_dir.parent / "lib" / "base"
    if base_dir.is_dir():
        sources.extend(sorted(base_dir.rglob("*.vala")))

    index = DocIndex(types={}, members={}, enum_members={})
    for path in sources:
        try:
            filename = path.relative_to(repo_root).as_posix()
        except ValueError:
            filename = path.name
        _collect_docs_from_source(path.read_text(encoding="utf-8"), index, filename)
    return index

def _collect_docs_from_source(source: str, index: DocIndex, filename: str):
    lines = source.split("\n")
    pending = None
    current_type = None
    current_is_enum = False
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("/**"):
            start_line = i + 1
            block = [line]
            while "*/" not in lines[i]:
                i += 1
                block.append(lines[i])
            pending = _parse_doc_block(block)
            pending.filename = filename
            pending.line = start_line
            i += 1
            continue

        if stripped == "" or stripped.startswith("//") or stripped.startswith("["):
            i += 1
            continue

        indent = len(line) - len(line.lstrip("\t"))

        type_match = DOC_TYPE_PATTERN.match(stripped)
        if indent == 1 and type_match is not None:
            current_type = type_match.group(2)
            current_is_enum = type_match.group(1) in ("enum", "errordomain")
            if pending is not None:
                index.types[current_type] = pending
            pending = None
            i += 1
            continue

        if pending is not None and current_type is not None and indent >= 2:
            if current_is_enum:
                member_match = DOC_ENUM_MEMBER_PATTERN.match(stripped)
                if member_match is not None:
                    index.enum_members[(current_type, member_match.group(1))] = pending
            else:
                name = _parse_member_name(stripped, current_type)
                if name is not None:
                    index.members[(current_type, name)] = pending
            pending = None
            i += 1
            continue

        pending = None
        i += 1

def _parse_member_name(stripped: str, type_name: str):
    if not stripped.startswith("public"):
        return None
    signal_match = DOC_SIGNAL_PATTERN.match(stripped)
    if signal_match is not None:
        return signal_match.group(1)
    brace = stripped.find("{")
    if brace != -1 and "(" not in stripped[:brace]:
        property_match = DOC_PROPERTY_PATTERN.match(stripped)
        if property_match is not None:
            return property_match.group(1)
    ctor_match = re.match(r"public\s+" + re.escape(type_name) + r"(?:\.(\w+))?\s*\(", stripped)
    if ctor_match is not None:
        return "new" if ctor_match.group(1) is None else ctor_match.group(1)
    method_match = DOC_METHOD_PATTERN.match(stripped)
    if method_match is not None:
        return method_match.group(1)
    return None

def _parse_doc_block(block: List[str]) -> DocComment:
    text_lines = []
    for raw in block:
        s = raw.strip()
        if s.startswith("/**"):
            s = s[3:]
        if s.endswith("*/"):
            s = s[:-2]
        s = s.strip()
        if s.startswith("*"):
            s = s[1:]
            if s.startswith(" "):
                s = s[1:]
        text_lines.append(s.rstrip())

    # Valadoc block tags: "@param <name> <description>" and "@return <description>".
    body_lines = []
    params = {}
    returns_lines = []
    target = body_lines
    for line in text_lines:
        param_match = re.match(r"@param\s+(\w+)\s*(.*)", line)
        return_match = re.match(r"@returns?\s*(.*)", line)
        if param_match is not None:
            params[param_match.group(1)] = [param_match.group(2)]
            target = params[param_match.group(1)]
            continue
        if return_match is not None:
            returns_lines = [return_match.group(1)]
            target = returns_lines
            continue
        target.append(line)

    def finish(lines):
        return _valadoc_to_markdown("\n".join(lines).strip())

    return DocComment(
        body=finish(body_lines),
        params={k: finish(v) for k, v in params.items()},
        returns=finish(returns_lines),
    )

def _valadoc_to_markdown(text: str) -> str:
    if not text:
        return text
    # Code blocks: {{{ ... }}} -> fenced ``` blocks.
    def code_block(match):
        code = match.group(1).strip("\n")
        return "\n```\n" + code + "\n```\n"
    text = re.sub(r"\{\{\{(.*?)\}\}\}", code_block, text, flags=re.DOTALL)
    # Note: {@link ...} cross-references are resolved later, at injection time,
    # where the full symbol table is available (see inject_documentation).
    # Inline monospace: ``x`` -> `x` (valadoc) is already markdown-compatible.
    # Bold: ''x'' -> **x**.
    text = re.sub(r"''(.+?)''", r"**\1**", text)
    # Italic: //x// -> *x*, but leave protocol-relative URLs (://) alone.
    text = re.sub(r"(?<![:/])//(?!/)(.+?)//", r"*\1*", text)
    return text

def inject_documentation(merged_namespace: ET.Element, docs: DocIndex):
    glib_signal_tag = f"{{{GLIB_NAMESPACE}}}signal"
    callable_tags = {
        f"{{{CORE_NAMESPACE}}}method",
        f"{{{CORE_NAMESPACE}}}constructor",
        f"{{{CORE_NAMESPACE}}}function",
        f"{{{CORE_NAMESPACE}}}virtual-method",
        glib_signal_tag,
    }
    property_tag = f"{{{CORE_NAMESPACE}}}property"
    member_tag = f"{{{CORE_NAMESPACE}}}member"
    doc_tag = f"{{{CORE_NAMESPACE}}}doc"

    # Build a symbol table so {@link ...} can be turned into real gi-docgen
    # cross-references. Keys use Vala-style names (underscores); values carry the
    # gi-docgen link kind and the GIR name (dashes for properties/signals).
    type_kinds = {}      # type_name -> gi-docgen kind ("class"/"iface"/"enum"/"error")
    member_kinds = {}    # (type_name, vala_member) -> (kind, gir_name)
    for type_elem in list(merged_namespace):
        type_name = type_elem.get("name")
        if type_name is None:
            continue
        tag = type_elem.tag.split("}")[-1]
        if tag == "class":
            type_kinds[type_name] = "class"
        elif tag == "interface":
            type_kinds[type_name] = "iface"
        elif tag == "enumeration":
            type_kinds[type_name] = "error" \
                if type_elem.get(f"{{{GLIB_NAMESPACE}}}error-domain") is not None \
                else "enum"
        for child in list(type_elem):
            cname = child.get("name")
            if cname is None:
                continue
            if child.tag == f"{{{CORE_NAMESPACE}}}method":
                member_kinds[(type_name, cname)] = ("method", cname)
            elif child.tag == f"{{{CORE_NAMESPACE}}}constructor":
                member_kinds[(type_name, cname)] = ("ctor", cname)
            elif child.tag == property_tag:
                member_kinds[(type_name, cname.replace("-", "_"))] = ("property", cname)
            elif child.tag == glib_signal_tag:
                member_kinds[(type_name, cname.replace("-", "_"))] = ("signal", cname)

    def resolve_links(text: str) -> str:
        def repl(match):
            target = re.sub(r"^Frida\.", "", match.group(1).strip())
            parts = target.split(".")
            if len(parts) == 1:
                kind = type_kinds.get(parts[0])
                if kind is not None:
                    return f"[{kind}@Frida.{parts[0]}]"
            elif len(parts) == 2:
                info = member_kinds.get((parts[0], parts[1]))
                if info is not None:
                    kind, gir_name = info
                    if kind == "method":
                        return f"[method@Frida.{parts[0]}.{gir_name}]"
                    if kind == "ctor":
                        return f"[ctor@Frida.{parts[0]}.{gir_name}]"
                    if kind == "property":
                        return f"[property@Frida.{parts[0]}:{gir_name}]"
                    if kind == "signal":
                        return f"[signal@Frida.{parts[0]}::{gir_name}]"
            return "`" + target + "`"
        return re.sub(r"\{@link\s+([^}]+)\}", repl, text)

    def set_doc(elem: ET.Element, text: str, filename: str = "<generated>",
                line: int = 0):
        if not text:
            return
        if elem.find(doc_tag) is not None:
            return
        doc = ET.Element(doc_tag)
        doc.set("xml:space", "preserve")
        # gi-docgen accesses filename/line unconditionally, so always emit them,
        # mirroring what g-ir-scanner produces for hand-written C.
        doc.set("filename", filename)
        doc.set("line", str(line))
        doc.text = resolve_links(text)
        elem.insert(0, doc)

    def member_doc(type_name: str, name: str):
        comment = docs.members.get((type_name, name))
        if comment is None:
            for suffix in ("_finish", "_sync"):
                if name.endswith(suffix):
                    comment = docs.members.get((type_name, name[: -len(suffix)]))
                    if comment is not None:
                        break
        return comment

    def apply_callable(elem: ET.Element, comment: DocComment):
        set_doc(elem, comment.body, comment.filename, comment.line)
        if comment.returns:
            rv = elem.find(f"{{{CORE_NAMESPACE}}}return-value")
            if rv is not None:
                set_doc(rv, comment.returns, comment.filename, comment.line)
        params_elem = elem.find(f"{{{CORE_NAMESPACE}}}parameters")
        if params_elem is not None and comment.params:
            for param in params_elem.findall(f"{{{CORE_NAMESPACE}}}parameter"):
                text = comment.params.get(param.get("name"))
                if text:
                    set_doc(param, text, comment.filename, comment.line)

    for type_elem in list(merged_namespace):
        type_name = type_elem.get("name")
        if type_name is None:
            continue
        tag = type_elem.tag.split("}")[-1]

        type_comment = docs.types.get(type_name)
        if type_comment is not None and tag in ("class", "interface", "enumeration"):
            set_doc(type_elem, type_comment.body, type_comment.filename,
                    type_comment.line)

        for child in list(type_elem):
            child_name = child.get("name")
            if child_name is None:
                continue
            if child.tag in callable_tags:
                lookup = child_name.replace("-", "_") if child.tag == glib_signal_tag \
                    else child_name
                comment = member_doc(type_name, lookup)
                if comment is not None:
                    apply_callable(child, comment)
                elif child.tag == f"{{{CORE_NAMESPACE}}}constructor" \
                        and child_name == "new":
                    # Vala's implicit default constructor; give it a sensible
                    # description rather than leaving it bare.
                    set_doc(child, f"Creates a new [class@Frida.{type_name}].")
            elif child.tag == property_tag:
                vala_name = child_name.replace("-", "_")
                comment = docs.members.get((type_name, vala_name))
                if comment is not None:
                    set_doc(child, comment.body, comment.filename, comment.line)
                    # The C API also exposes the property through getter/setter
                    # methods; give them the same description.
                    accessors = {"get_" + vala_name, "set_" + vala_name}
                    for sibling in type_elem:
                        if sibling.tag == f"{{{CORE_NAMESPACE}}}method" \
                                and sibling.get("name") in accessors:
                            set_doc(sibling, comment.body, comment.filename,
                                    comment.line)
            elif child.tag == member_tag:
                comment = docs.enum_members.get((type_name, child_name.upper()))
                if comment is not None:
                    set_doc(child, comment.body, comment.filename, comment.line)

def emit_vapi(api, output_dir):
    with OutputFile(output_dir / f"frida-core-{api.version}.vapi") as output_vapi_file:
        output_vapi_file.write("[CCode (cheader_filename = \"frida-core.h\", cprefix = \"Frida\", lower_case_cprefix = \"frida_\")]")
        output_vapi_file.write("\nnamespace Frida {")
        output_vapi_file.write("\n\tpublic static void init ();")
        output_vapi_file.write("\n\tpublic static void init_with_runtime (Frida.Runtime runtime);")
        output_vapi_file.write("\n\tpublic static void shutdown ();")
        output_vapi_file.write("\n\tpublic static void deinit ();")
        output_vapi_file.write("\n\tpublic static unowned GLib.MainContext get_main_context ();")

        for delegate in referenced_delegates(api):
            output_vapi_file.write("\n\t" + delegate)

        for object_type in api.object_types:
            output_vapi_file.write("\n\n\t%s" % object_type.vapi_declaration)
            sections = []
            if len(object_type.vapi_properties) > 0:
                sections.append("\n\t\t" + "\n\t\t".join(object_type.vapi_properties))
            if object_type.vapi_constructor is not None:
                sections.append("\n\t\t" + object_type.vapi_constructor)
            if len(object_type.vapi_methods) > 0:
                sections.append("\n\t\t" + "\n\t\t".join(object_type.vapi_methods))
            if len(object_type.vapi_signals) > 0:
                sections.append("\n\t\t" + "\n\t\t".join(object_type.vapi_signals))
            output_vapi_file.write("\n".join(sections))
            output_vapi_file.write("\n\t}")

        output_vapi_file.write("\n")
        for func in api.functions:
            output_vapi_file.write("\n" + func.vapi_declaration)

        for enum in api.error_types:
            output_vapi_file.write("\n\n\t%s\n\t\t" % enum.vapi_declaration)
            output_vapi_file.write("\n\t\t".join(enum.vapi_members))
            output_vapi_file.write("\n\t}")

        for enum in api.enum_types:
            output_vapi_file.write("\n\n\t%s\n\t\t" % enum.vapi_declaration)
            output_vapi_file.write("\n\t\t".join(enum.vapi_members))
            output_vapi_file.write("\n\t}")

        output_vapi_file.write("\n}\n")

    with OutputFile(output_dir / f"frida-core-{api.version}.deps") as output_deps_file:
        output_deps_file.write("glib-2.0\n")
        output_deps_file.write("gobject-2.0\n")
        output_deps_file.write("gio-2.0\n")
        output_deps_file.write("json-glib-1.0\n")

def referenced_delegates(api) -> List[str]:
    members = []
    for object_type in api.object_types:
        members.extend(object_type.vapi_properties)
        members.extend(object_type.vapi_methods)
        members.extend(object_type.vapi_signals)
        if object_type.vapi_constructor is not None:
            members.append(object_type.vapi_constructor)
    for func in api.functions:
        members.append(func.vapi_declaration)
    text = "\n".join(members)

    return [declaration for name, declaration in api.delegates
            if re.search(r"\bFrida\." + name + r"\b", text) is not None]

def strip_internal_interfaces(declaration: str) -> str:
    head, sep, rest = declaration.partition(" : ")
    if not sep:
        return declaration

    bases_part, brace_sep, tail = rest.partition("{")

    kept = [b.strip() for b in bases_part.split(",") if b.strip() not in INTERNAL_INTERFACES]
    if not kept:
        return f"{head} {brace_sep}{tail}"

    return f"{head} : {', '.join(kept)} {brace_sep}{tail}"

def emit_symbol_maps_from_source(output_dir: Path):
    with OutputFile(output_dir / 'frida-core.version') as f:
        f.write("{\n")
        f.write("  global:\n")
        f.write("    frida_*;\n")
        f.write("    _frida_*;\n")
        f.write("\n")
        f.write("  local:\n")
        f.write("    *;\n")
        f.write("};\n")

def parse_api(frida_version, frida_version_components, api_version, toplevel_code, core_header, core_vapi, base_header, base_vapi):
    all_headers = core_header + "\n" + base_header

    all_enum_names = [m.group(1) for m in re.finditer(r"^\t+public\s+enum\s+(\w+)\s+", toplevel_code + "\n" + base_vapi, re.MULTILINE)]
    enum_types = []

    base_public_types = {
        "SpawnGatingOptions": "ProcessMatchOptions",
        "FrontmostQueryOptions": "SpawnOptions",
        "ApplicationQueryOptions": "FrontmostQueryOptions",
        "ProcessQueryOptions": "ApplicationQueryOptions",
        "SessionOptions": "ProcessQueryOptions",
        "ScriptOptions": "Script",
        "SnapshotOptions": "Script",
        "PeerOptions": "ScriptOptions",
        "Relay": "PeerOptions",
        "PortalOptions": "Relay",
        "RpcClient": "PortalMembership",
        "RpcPeer": "RpcClient",
        "EndpointParameters": "PortalService",
        "AuthenticationService": "EndpointParameters",
        "StaticAuthenticationService": "AuthenticationService",
        "WebRequestHandler": "StaticAuthenticationService",
        "WebRequest": "WebRequestHandler",
        "WebResponse": "WebRequest",
    }
    internal_type_prefixes = [
        "AgentMessageKind",
        "DrainStatus",
        "Fruity",
        "HostSession",
        "LinuxCompat32Syscall",
        "LinuxSyscall",
        "MessageType",
        "PeerSetup",
        "PortConflictBehavior",
        "RecordAction",
        "ResultCode",
        "SpawnStartState",
        "State",
        "StringTerminator",
        "UnloadPolicy",
        "WebService",
        "Winjector"
    ]
    seen_enum_names = set()
    for enum_name in all_enum_names:
        if enum_name in seen_enum_names:
            continue
        seen_enum_names.add(enum_name)

        is_public = True
        for prefix in internal_type_prefixes:
            if enum_name.startswith(prefix):
                is_public = False
                break

        if is_public:
            enum_types.append(ApiEnum(enum_name))

    enum_by_name = {}
    for enum in enum_types:
        enum_by_name[enum.name] = enum
    for enum in enum_types:
        for m in re.finditer(r"typedef\s+enum\s+.*?\s+(\w+);", all_headers, re.DOTALL):
            if m.group(1) == enum.c_name:
                enum.c_definition = beautify_cenum(m.group(0))
                break

    error_types = [ApiEnum(m.group(1)) for m in re.finditer(r"^\t+public\s+errordomain\s+(\w+)\s+", base_vapi, re.MULTILINE)]
    error_by_name = {}
    for enum in error_types:
        error_by_name[enum.name] = enum
    for enum in error_types:
        for m in re.finditer(r"typedef\s+enum\s+.*?\s+(\w+);", base_header, re.DOTALL):
            if m.group(1) == enum.c_name:
                enum.c_definition = beautify_cenum(m.group(0))
                break

    object_types = parse_vala_object_types(toplevel_code)

    pending_public_types = set(base_public_types.keys())
    base_object_types = parse_vala_object_types(base_vapi)
    while len(pending_public_types) > 0:
        for potential_type in base_object_types:
            name = potential_type.name
            if name in pending_public_types:
                insert_after = base_public_types[name]
                for i, t in enumerate(object_types):
                    if t.name == insert_after:
                        object_types.insert(i + 1, potential_type)
                        pending_public_types.remove(name)

    object_type_by_name = {}
    for klass in object_types:
        object_type_by_name[klass.name] = klass
    seen_cfunctions = set()
    seen_cdelegates = set()
    for object_type in sorted(object_types, key=lambda klass: len(klass.c_name_lc), reverse=True):
        for m in re.finditer(r"^.*?\s+" + object_type.c_name_lc + r"_(\w+)\s+[^;]+;", all_headers, re.MULTILINE):
            method_cprototype = beautify_cprototype(m.group(0))
            if method_cprototype.startswith("VALA_EXTERN "):
                method_cprototype = method_cprototype[12:]
            method_name = m.group(1)
            method_cname_lc = object_type.c_name_lc + '_' + method_name
            if method_cname_lc not in seen_cfunctions:
                seen_cfunctions.add(method_cname_lc)
                if method_name != 'construct':
                    if (object_type.c_name + '*') in m.group(0):
                        if method_name == 'new' or method_name.startswith('new_'):
                            object_type.c_constructors.append(method_cprototype)
                        elif method_name.startswith('get_') and not any(arg in method_cprototype for arg in ['GAsyncReadyCallback', 'GError ** error']):
                            object_type.property_names.append(method_name[4:])
                            object_type.c_getter_prototypes.append(method_cprototype)
                        else:
                            object_type.method_names.append(method_name)
                            object_type.c_method_prototypes.append(method_cprototype)
                    elif method_name == 'get_type':
                        object_type.c_get_type = method_cprototype.replace("G_GNUC_CONST ;", "G_GNUC_CONST;")
        for d in re.finditer(r"^typedef.+?\(\*(" + object_type.c_name + r".+?)\) \(.+\);$", all_headers, re.MULTILINE):
            delegate_cname = d.group(1)
            if delegate_cname not in seen_cdelegates:
                seen_cdelegates.add(delegate_cname)
                object_type.c_delegate_typedefs.append(beautify_cprototype(d.group(0)))
        if object_type.kind == 'interface':
            for m in re.finditer("^(struct _" + object_type.c_name + "Iface {[^}]+};)$", all_headers, re.MULTILINE):
                object_type.c_iface_definition = beautify_cinterface(m.group(1))

    current_enum = None
    current_object_type = None
    ignoring = False
    for line in (core_vapi + base_vapi).split("\n"):
        stripped_line = line.strip()
        level = 0
        for c in line:
            if c == '\t':
                level += 1
            else:
                break
        if level == 0:
            pass
        elif level == 1:
            if ignoring:
                if stripped_line == "}":
                    ignoring = False
            else:
                if stripped_line.startswith("public class Promise") \
                        or stripped_line.startswith("public interface Future") \
                        or stripped_line.startswith("public class CF"):
                    ignoring = True
                elif stripped_line.startswith("public enum") or stripped_line.startswith("public errordomain"):
                    name = re.match(r"^public (?:enum|errordomain) (\w+) ", stripped_line).group(1)
                    if name in enum_by_name:
                        current_enum = enum_by_name[name]
                        current_enum.vapi_declaration = stripped_line
                    elif name in error_by_name:
                        current_enum = error_by_name[name]
                        current_enum.vapi_declaration = stripped_line
                    else:
                        ignoring = True
                elif (match := OBJECT_TYPE_PATTERN.match(stripped_line)) is not None:
                    name = match.group(3)
                    if name not in object_type_by_name:
                        ignoring = True
                    else:
                        current_object_type = object_type_by_name[name]
                        current_object_type.vapi_declaration = strip_internal_interfaces(stripped_line)
                elif stripped_line == "}":
                    current_enum = None
                    current_object_type = None
        elif current_enum is not None:
            current_enum.vapi_members.append(stripped_line)
        elif current_object_type is not None and stripped_line.startswith("public"):
            if stripped_line.startswith("public " + current_object_type.name + " (") or stripped_line.startswith("public static Frida." + current_object_type.name + " @new ("):
                if len(current_object_type.c_constructors) > 0:
                    current_object_type.vapi_constructor = stripped_line
            elif stripped_line.startswith("public signal"):
                current_object_type.vapi_signals.append(stripped_line)
            elif "{ get" in stripped_line:
                name = re.match(r".+?(\w+)\s+{", stripped_line).group(1)
                current_object_type.vapi_properties.append(stripped_line)
            else:
                m = re.match(r".+?(\w+)\s+\(", stripped_line)
                if m is not None:
                    name = m.group(1)
                    if not name.startswith("_") and name != 'dispose':
                        current_object_type.vapi_methods.append(stripped_line)
    for object_type in object_types:
        object_type.sort_members()
    for enum in enum_types:
        if enum.vapi_declaration is None:
            m = re.match(r".+\s+(public\s+enum\s+" + enum.name + r"\s+{)(.+?)}", base_vapi, re.MULTILINE | re.DOTALL)
            enum.vapi_declaration = m.group(1)
            enum.vapi_members.extend([line.lstrip() for line in m.group(2).strip().split("\n")])

    functions = [f for f in parse_vapi_functions(core_vapi + base_vapi) if function_is_public(f.name)]
    for f in functions:
        m = re.search(r"^(?:VALA_EXTERN )?[\w\*]+ frida_{}.+?;".format(f.name), all_headers, re.MULTILINE | re.DOTALL)
        prototype = beautify_cprototype(m.group(0))
        if prototype.startswith("VALA_EXTERN "):
            prototype = prototype[12:]
        f.c_prototype = prototype

    delegates = [(m.group(1), m.group(0).strip())
                 for m in re.finditer(r"^\tpublic delegate .+?\b(\w+) \(.*\);$", core_vapi + base_vapi, re.MULTILINE)]

    return ApiSpec(frida_version, frida_version_components, api_version, object_types, functions, enum_types, error_types,
                   delegates)

def function_is_public(name):
    return not name.startswith("_") and \
            not name.startswith("throw_") and \
            name not in MANUALLY_DECLARED_FUNCTIONS and \
            name not in {
                "on_pending_garbage",
                "generate_certificate",
                "get_dbus_context",
                "invalidate_dbus_context",
                "make_json_reader",
                "make_json_reader_taking_node",
                "make_parameters_dict",
                "compute_system_parameters",
                "parse_control_address",
                "parse_cluster_address",
                "parse_socket_address",
                "negotiate_connection",
                "check_kernel_version",
                "get_syscall_signatures",
                "get_compat32_syscall_signatures",
                "get_xnu_mach_traps",
                "get_xnu_bsd_syscalls",
                "make_stdio_pipes",
                "make_stdio_pipe",
            }

MANUALLY_DECLARED_FUNCTIONS = {
    "init",
    "init_with_runtime",
    "shutdown",
    "deinit",
    "get_main_context",
    "unref",
    "version",
    "version_string",
}

def parse_vala_object_types(source) -> List[ApiObjectType]:
    return [ApiObjectType(m.group(3), m.group(2)) for m in OBJECT_TYPE_PATTERN.finditer(source, re.MULTILINE)]

def parse_vapi_functions(vapi) -> List[ApiFunction]:
    return [ApiFunction(m.group(1), m.group(0)) for m in re.finditer(r"^\tpublic static .+ (\w+) \(.+;", vapi, re.MULTILINE)]

@dataclass
class ApiSpec:
    frida_version: str
    frida_version_components: List[int]
    version: str
    object_types: List[ApiObjectType]
    functions: List[ApiFunction]
    enum_types: List[ApiEnum]
    error_types: List[ApiEnum]
    delegates: List[tuple]

class ApiEnum:
    def __init__(self, name):
        self.name = name
        self.name_lc = camel_identifier_to_lc(self.name)
        self.name_uc = camel_identifier_to_uc(self.name)
        self.c_name = 'Frida' + name
        self.c_name_lc = camel_identifier_to_lc(self.c_name)
        self.c_definition = None
        self.vapi_declaration = None
        self.vapi_members = []

class ApiObjectType:
    def __init__(self, name, kind):
        self.name = name
        self.name_lc = camel_identifier_to_lc(self.name)
        self.name_uc = camel_identifier_to_uc(self.name)
        self.kind = kind
        self.property_names = []
        self.method_names = []
        self.c_name = 'Frida' + name
        self.c_name_lc = camel_identifier_to_lc(self.c_name)
        self.c_get_type = None
        self.c_constructors = []
        self.c_getter_prototypes = []
        self.c_method_prototypes = []
        self.c_delegate_typedefs = []
        self.c_iface_definition = None
        self.vapi_declaration = None
        self.vapi_signals = []
        self.vapi_properties = []
        self.vapi_constructor = None
        self.vapi_methods = []

    def sort_members(self):
        self.vapi_properties = fuzzysort(self.vapi_properties, self.property_names)
        self.vapi_methods = fuzzysort(self.vapi_methods, self.method_names)

class ApiFunction:
    def __init__(self, name, vapi_declaration):
        self.name = name
        self.c_prototype = None
        self.vapi_declaration = vapi_declaration

    def __repr__(self):
        return "ApiFunction(name=\"{}\")".format(self.name)

def camel_identifier_to_lc(camel_identifier):
    result = ""
    for c in camel_identifier:
        if c.istitle() and len(result) > 0:
            result += '_'
        result += c.lower()
    return result

def camel_identifier_to_uc(camel_identifier):
    result = ""
    for c in camel_identifier:
        if c.istitle() and len(result) > 0:
            result += '_'
        result += c.upper()
    return result

def beautify_cenum(cenum):
    return cenum.replace("  ", " ").replace("\t", "  ")

def beautify_cprototype(cprototype):
    result = cprototype.replace("\n", "")
    result = re.sub(r"\s+", " ", result)
    result = re.sub(r"([a-z0-9])\*", r"\1 *", result)
    result = re.sub(r"\(\*", r"(* ", result)
    result = re.sub(r"(, )void \* (.+?)_target\b", r"\1gpointer \2_data", result)
    result = result.replace("void * user_data", "gpointer user_data")
    result = result.replace("gpointer func_target", "gpointer user_data")
    result = result.replace("_length1", "_length")
    result = result.replace(" _callback_,", " callback,")
    result = result.replace(" _user_data_", " user_data")
    result = result.replace(" _res_", " result")
    return result

def beautify_cinterface(iface):
    lines = iface.split("\n")

    header = lines[0]
    body = ["  " + beautify_cprototype(line.lstrip()) for line in lines[1:-1]]
    footer = lines[-1]

    return "\n".join([header, *body, footer])

def fuzzysort(items, keys):
    result = []
    remaining = list(items)
    for key in keys:
        for item in remaining:
            if (" " + key + " ") in item:
                remaining.remove(item)
                result.append(item)
                break
    result.extend(remaining)
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
            existing_contents = self._output_path.read_text(encoding='utf-8')
            if existing_contents == result:
                return False
        self._output_path.write_text(result, encoding='utf-8')
        return False


if __name__ == '__main__':
    main()
