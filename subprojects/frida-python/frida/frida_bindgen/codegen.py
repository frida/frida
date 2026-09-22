from __future__ import annotations

import ast
import re
import textwrap
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple, Union

from frida_bindgen_core import Procedure, Type
from frida_bindgen_core.naming import to_pascal_case

from .model import (
    Enumeration,
    InterfaceObjectType,
    Method,
    Model,
    ObjectType,
    Parameter,
    to_snake_case,
)

ASSETS_DIR = Path(__file__).resolve().parent / "assets"
CODEGEN_MACROS_H = (ASSETS_DIR / "codegen_macros.h").read_text(encoding="utf-8")
CODEGEN_TYPEDEFS_H = (ASSETS_DIR / "codegen_typedefs.h").read_text(encoding="utf-8")
CODEGEN_STRUCTS_H = (ASSETS_DIR / "codegen_structs.h").read_text(encoding="utf-8")
CODEGEN_GOBJECT_PROTOTYPES = (ASSETS_DIR / "codegen_gobject_prototypes.h").read_text(encoding="utf-8")
CODEGEN_GOBJECT_GLOBALS = (ASSETS_DIR / "codegen_gobject_globals.c").read_text(encoding="utf-8")
CODEGEN_GOBJECT_METHODS = (ASSETS_DIR / "codegen_gobject_methods.c").read_text(encoding="utf-8")
FACADE_INTERFACE = (ASSETS_DIR / "facade_interface.py").read_text(encoding="utf-8")
FACADE_INTERFACE_AIO = (ASSETS_DIR / "facade_interface_aio.py").read_text(encoding="utf-8")


def read_asset(name: str) -> str:
    return (ASSETS_DIR / name).read_text(encoding="utf-8")


FACADE_TYPING_IMPORTS = """\
from typing import (Any, Callable, Dict, List, Literal, Mapping, Optional, Tuple, Sequence, Union, TypeVar, cast,
                    overload)

if sys.version_info >= (3, 11):
    from typing import NotRequired, ParamSpec, TypedDict
else:
    from typing_extensions import NotRequired, ParamSpec, TypedDict"""


def generate_facade_preludes(model: Model) -> List[str]:
    lines = []
    for asset in model.customizations.facade_preludes:
        lines += ["", read_asset(asset).strip(), ""]
    return lines


FACADE_RUNTIME = """
_to_json = json.dumps


def _wrap(impl):
    if impl is None:
        return None
    cls = _WRAPPERS.get(type(impl).__name__)
    if cls is None:
        return impl
    wrapper = cls.__new__(cls)
    wrapper._impl = impl
    setup = getattr(wrapper, "_setup", None)
    if setup is not None:
        setup()
    return wrapper


def _unwrap(obj):
    if obj is None:
        return None
    return getattr(obj, "_impl", obj)


def _make_signal_handler(callback):
    def handler(*args):
        return callback(*[_wrap(arg) for arg in args])

    handler._frida_original = callback
    handler.__signature__ = inspect.signature(callback)
    return handler


def _to_envp(value):
    if isinstance(value, dict):
        return [f"{key}={val}" for key, val in value.items()]
    return value


def _make_options(cls, values, selectors):
    options = cls()
    _apply_settings(options, values, selectors)
    return options


def _apply_settings(impl, values, selectors):
    for name, value in values.items():
        if value is None:
            continue
        select = selectors.get(name)
        if select is None:
            setattr(impl, name, _unwrap(value))
        elif isinstance(value, dict):
            add = getattr(impl, select)
            for key, element in value.items():
                add(key, _unwrap(element))
        else:
            add = getattr(impl, select)
            for element in value:
                add(_unwrap(element))


_current_cancellable = threading.local()


def _current_cancellable_get():
    stack = getattr(_current_cancellable, "stack", None)
    return stack[-1] if stack else None


def _invoke(method, args):
    completed = threading.Event()
    outcome = {}

    def on_complete(result, error):
        outcome["result"] = result
        outcome["error"] = error
        completed.set()

    cancellable = _current_cancellable_get()
    if cancellable is None:
        method(*args, on_complete)
        completed.wait()
    else:
        def on_cancelled():
            completed.set()

        method(*args, on_complete, cancellable._impl)
        cancellable._impl.on("cancelled", on_cancelled)
        try:
            completed.wait()
        finally:
            cancellable._impl.off("cancelled", on_cancelled)

    if "error" not in outcome:
        raise _frida.OperationCancelledError("operation was cancelled")
    error = outcome["error"]
    if error is not None:
        raise error
    return outcome["result"]
"""

AIO_RUNTIME = """
_to_json = json.dumps


def _wrap(impl):
    if impl is None:
        return None
    cls = _WRAPPERS.get(type(impl).__name__)
    if cls is None:
        return impl
    wrapper = cls.__new__(cls)
    wrapper._impl = impl
    setup = getattr(wrapper, "_setup", None)
    if setup is not None:
        setup()
    return wrapper


def _unwrap(obj):
    if obj is None:
        return None
    return getattr(obj, "_impl", obj)


def _make_signal_handler(callback):
    def handler(*args):
        return callback(*[_wrap(arg) for arg in args])

    handler._frida_original = callback
    handler.__signature__ = inspect.signature(callback)
    return handler


def _to_envp(value):
    if isinstance(value, dict):
        return [f"{key}={val}" for key, val in value.items()]
    return value


def _make_options(cls, values, selectors):
    options = cls()
    _apply_settings(options, values, selectors)
    return options


def _apply_settings(impl, values, selectors):
    for name, value in values.items():
        if value is None:
            continue
        select = selectors.get(name)
        if select is None:
            setattr(impl, name, _unwrap(value))
        elif isinstance(value, dict):
            add = getattr(impl, select)
            for key, element in value.items():
                add(key, _unwrap(element))
        else:
            add = getattr(impl, select)
            for element in value:
                add(_unwrap(element))


_current_cancellable: contextvars.ContextVar = contextvars.ContextVar("frida_current_cancellable", default=None)


def _dispatch(loop, callback, *args):
    try:
        loop.call_soon_threadsafe(callback, *args)
    except RuntimeError:
        pass


async def _invoke(method, args):
    loop = asyncio.get_running_loop()
    future = loop.create_future()
    ambient = _current_cancellable.get()
    cancellable = ambient._impl if ambient is not None else _frida.Cancellable()

    def deliver(result, error):
        if future.done():
            return
        if error is not None:
            future.set_exception(error)
        else:
            future.set_result(result)

    def on_complete(result, error):
        _dispatch(loop, deliver, result, error)

    method(*args, on_complete, cancellable)

    try:
        return await future
    except asyncio.CancelledError:
        cancellable.cancel()
        raise
"""


def generate_all(model: Model) -> Dict[str, str]:
    return {
        "py": generate_py(model),
        "aio": generate_aio(model),
        "pyi": generate_extension_pyi(model),
        "c": generate_extension_c(model),
    }


def generate_py(model: Model) -> str:
    lines = [
        "from . import _frida",
        "import dataclasses",
        "import fnmatch",
        "import functools",
        "import inspect",
        "import time",
        "import json",
        "import sys",
        "import threading",
        "import traceback",
        FACADE_TYPING_IMPORTS,
        "",
        "",
        *generate_py_exception_reexports(model),
        *generate_facade_preludes(model),
        "",
        FACADE_RUNTIME.strip(),
        "",
        "",
        FACADE_INTERFACE.strip(),
    ]

    for helper in facade_module_helpers(model, aio=False):
        lines += ["", "", read_asset(helper).strip()]

    for otype in facade_object_types(model):
        lines.append("")
        lines.append("")
        lines.append(generate_py_class(otype, model))

    lines.append("")
    lines.append("")
    lines.append("_WRAPPERS = {")
    for otype in facade_object_types(model):
        lines.append(f'    "{otype.py_name}": {otype.py_name},')
    lines.append("}")
    lines.append("")
    lines.append("")
    lines.append("from . import aio")
    for asset in model.customizations.facade_epilogues:
        lines.append("")
        lines.append(read_asset(asset).strip())
    lines.append("")
    lines.append("core = sys.modules[__name__]")
    lines.append('sys.modules[__name__ + ".core"] = core')
    lines.append("")

    return "\n".join(lines)


def facade_object_types(model: Model) -> List[ObjectType]:
    return [t for t in model.regular_object_types if not t.is_frida_options]


def facade_module_helpers(model: Model, aio: bool) -> List[str]:
    seen = set()
    result = []
    for otype in model.object_types.values():
        custom_code = otype.custom_code
        if custom_code is None or custom_code.helpers is None:
            continue
        asset = custom_code.helpers[1] if aio else custom_code.helpers[0]
        if asset not in seen:
            seen.add(asset)
            result.append(asset)
    return result


def generate_aio(model: Model) -> str:
    lines = [
        "from . import _frida",
        "import asyncio",
        "import contextvars",
        "import time",
        "import dataclasses",
        "import fnmatch",
        "import functools",
        "import inspect",
        "import json",
        "import sys",
        "import traceback",
        FACADE_TYPING_IMPORTS,
        "",
        "",
        *generate_py_exception_reexports(model),
        *generate_facade_preludes(model),
        "",
        AIO_RUNTIME.strip(),
        "",
        "",
        FACADE_INTERFACE_AIO.strip(),
    ]

    for helper in facade_module_helpers(model, aio=True):
        lines += ["", "", read_asset(helper).strip()]

    for otype in facade_object_types(model):
        lines.append("")
        lines.append("")
        lines.append(generate_aio_class(otype, model))

    lines.append("")
    lines.append("")
    lines.append("_WRAPPERS = {")
    for otype in facade_object_types(model):
        lines.append(f'    "{otype.py_name}": {otype.py_name},')
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


def generate_aio_class(otype: ObjectType, model: Model) -> str:
    if implementable_interface(otype):
        return generate_aio_interface_facade(otype)

    members = [generate_facade_init(otype)]
    if otype.signals and not otype.provides_signals:
        members.append(generate_facade_signals())
    for method in otype.methods:
        if method.suppress_facade:
            continue
        member = generate_py_property(method, model) or generate_aio_method(method, model)
        if member is not None:
            members.append(member)
    custom_code = otype.custom_code
    if custom_code is not None and custom_code.members is not None:
        members.append(generate_custom_members(custom_code.members, aio=True))
    repr_member = generate_facade_repr(otype)
    if repr_member is not None:
        members.append(repr_member)
    if not members:
        members.append("    pass")

    members = apply_signal_overloads(members, otype, model)

    return f"class {otype.py_name}:\n" + "\n\n".join(members)


def generate_custom_members(members: Tuple[str, str], aio: bool) -> str:
    asset = members[1] if aio else members[0]
    return textwrap.indent(read_asset(asset).strip(), "    ")


def generate_aio_method(method: Method, model: Model) -> Optional[str]:
    if method.is_property_accessor:
        return None
    if method.as_property:
        return generate_facade_bool_property(method)
    custom_logic = method.custom_logic
    if custom_logic is not None:
        return generate_custom_facade_method(method, custom_logic, model, awaitable=method.is_async)
    if method.is_async:
        return generate_aio_async_method(method, model)
    return generate_py_sync_method(method, model)


def generate_facade_bool_property(method: Method) -> str:
    return f"""    @property
    def {method.name}(self) -> bool:
        return self._impl.{method.name}()"""


def generate_aio_async_method(method: Method, model: Model) -> Optional[str]:
    parts = build_facade_async_parts(method, model)
    if parts is None:
        return None
    signature, args = parts

    call = f"await _invoke(self._impl.{method.name}, ({args}))"
    if method.return_value is not None:
        call = wrap_result(call, method.return_value.type, model)

    ret = "None" if method.return_value is None else pyi_type(method.return_value.type, model)

    return f"""    async def {method.name}({signature}) -> {ret}:
        return {call}"""


def generate_custom_facade_method(
    method: Method, logic: Union[str, Tuple[str, str]], model: Model, awaitable: bool
) -> str:
    signature = ", ".join(["self"] + method.custom_facade_params)
    args = facade_call_args(method, model)
    if method.is_async:
        invoke = f"_invoke(self._impl.{method.name}, ({args}))"
        call = f"await {invoke}" if awaitable else invoke
    else:
        call = f"self._impl.{method.name}({args})"
    if method.return_value is not None:
        call = wrap_result(call, method.return_value.type, model)

    if isinstance(logic, tuple):
        logic = logic[1] if awaitable else logic[0]
    body = textwrap.indent(logic.strip(), " " * 8)
    keyword = "async def" if awaitable else "def"

    ret = "None" if method.return_value is None else pyi_type(method.return_value.type, model)

    return f"""    {keyword} {method.name}({signature}) -> {ret}:
{body}
        return {call}"""


def facade_call_args(method: Method, model: Model) -> str:
    takes_kwargs = any(param.split(":")[0].strip() == "**kwargs" for param in method.custom_facade_params)
    args = []
    for param in method.input_parameters:
        if param.type.name == "Gio.Cancellable":
            continue
        options = resolve_options_type(param.type, model)
        if options is not None and takes_kwargs:
            args.append(make_options_expr(options))
        else:
            args.append(param.name)
    return "".join(arg + ", " for arg in args)


def generate_py_exception_reexports(model: Model) -> List[str]:
    names = [f"{member.js_name}Error" for member in model.error_domain.members]
    names.append("OperationCancelledError")
    return [f"{name} = _frida.{name}" for name in names]


def generate_py_class(otype: ObjectType, model: Model) -> str:
    if implementable_interface(otype):
        return generate_interface_facade(otype)

    members = [generate_facade_init(otype)]
    if otype.signals and not otype.provides_signals:
        members.append(generate_facade_signals())
    for method in otype.methods:
        if method.suppress_facade:
            continue
        member = generate_py_property(method, model) or generate_py_method(method, model)
        if member is not None:
            members.append(member)
    custom_code = otype.custom_code
    if custom_code is not None and custom_code.members is not None:
        members.append(generate_custom_members(custom_code.members, aio=False))
    repr_member = generate_facade_repr(otype)
    if repr_member is not None:
        members.append(repr_member)
    if not members:
        members.append("    pass")

    members = apply_signal_overloads(members, otype, model)

    return f"class {otype.py_name}:\n" + "\n\n".join(members)


def generate_interface_facade(otype: ObjectType) -> str:
    return f"""class {otype.py_name}(_Implementation):
    def __init__(self):
        self._impl = _frida.{otype.py_name}(self)"""


def generate_aio_interface_facade(otype: ObjectType) -> str:
    return f"""class {otype.py_name}(_Implementation):
    def __init__(self):
        self._loop = asyncio.get_event_loop()
        self._impl = _frida.{otype.py_name}(self)"""


def generate_facade_init(otype: ObjectType) -> str:
    custom_logic = otype.constructor_custom_logic
    if custom_logic is not None:
        signature = ", ".join(["self"] + otype.constructor_custom_params)
        body = textwrap.indent(custom_logic.strip(), " " * 8)
        return f"""    def __init__({signature}):
{body}"""

    return f"""    def __init__(self, *args, **kwargs):
        self._impl = _frida.{otype.py_name}(*[_unwrap(a) for a in args])
        _apply_settings(self._impl, kwargs, {build_option_selectors(otype)})
        setup = getattr(self, "_setup", None)
        if setup is not None:
            setup()"""


def generate_facade_signals() -> str:
    return """    def on(self, signal: str, callback: Callable[..., Any]) -> None:
        self._impl.on(signal, _make_signal_handler(callback))

    def off(self, signal: str, callback: Callable[..., Any]) -> None:
        self._impl.off(signal, callback)"""


def facade_prelude_names(model: Model) -> Set[str]:
    names = set()
    for asset in model.customizations.facade_preludes:
        for node in ast.parse(read_asset(asset)).body:
            if isinstance(node, ast.ClassDef):
                names.add(node.name)
            elif isinstance(node, ast.Assign):
                names.update(t.id for t in node.targets if isinstance(t, ast.Name))
    return names


def signal_callback_alias(otype: ObjectType, signal) -> str:
    return f"{otype.py_name}{to_pascal_case(signal.name.replace('-', '_'))}Callback"


def signal_overload_block(otype: ObjectType, model: Model, name: str) -> Optional[str]:
    aliases = facade_prelude_names(model)
    lines = []
    for signal in otype.signals:
        alias = signal_callback_alias(otype, signal)
        if alias in aliases:
            lines.append("    @overload")
            lines.append(f'    def {name}(self, signal: Literal["{signal.name}"], callback: {alias}) -> None: ...')
    return "\n".join(lines) if len(lines) > 2 else None


def apply_signal_overloads(members: List[str], otype: ObjectType, model: Model) -> List[str]:
    result = []
    for member in members:
        for name in ("on", "off"):
            block = signal_overload_block(otype, model, name)
            if block is None:
                continue
            pattern = re.compile(rf"^    (?:async )?def {name}\(self, signal", re.MULTILINE)
            match = pattern.search(member)
            if match is not None:
                member = member[: match.start()] + block + "\n" + member[match.start() :]
        result.append(member)
    return result


def facade_repr_property_names(otype: ObjectType) -> List[str]:
    names = []
    for method in otype.methods:
        name = property_name_from_accessor(method)
        if name is None or property_getter_marshal(method) is None:
            continue
        type_name = method.return_value.type.name
        is_scalar = (
            type_name == "utf8"
            or type_name == "gboolean"
            or type_name in PYARG_NUMERIC_FORMATS
            or resolve_enumeration(method.return_value.type, otype.model) is not None
        )
        if is_scalar:
            names.append(name)
    return names


def generate_facade_repr(otype: ObjectType) -> Optional[str]:
    names = facade_repr_property_names(otype)
    if not names:
        return None
    fields = ", ".join(f"{name}={{self.{name}!r}}" for name in names)
    return f'''    def __repr__(self):
        return f"{otype.py_name}({fields})"'''


def generate_py_property(method: Method, model: Model) -> Optional[str]:
    name = property_name_from_accessor(method)
    if name is None or property_getter_marshal(method) is None:
        return None

    access = wrap_result(f"self._impl.{name}", method.return_value.type, model)
    ret = pyi_type(method.return_value.type, model)
    prop = f"""    @property
    def {name}(self) -> {ret}:
        return {access}"""

    set_method = next((m for m in method.object_type.methods if m.name == f"set_{name}"), None)
    if set_method is not None and property_setter_supported(set_method):
        prop += f"""

    @{name}.setter
    def {name}(self, value: {ret}) -> None:
        self._impl.{name} = _unwrap(value)"""

    return prop


def generate_py_method(method: Method, model: Model) -> Optional[str]:
    if method.is_property_accessor:
        return None
    if method.as_property:
        return generate_facade_bool_property(method)
    custom_logic = method.custom_logic
    if custom_logic is not None:
        return generate_custom_facade_method(method, custom_logic, model, awaitable=False)
    if method.is_async:
        return generate_py_async_method(method, model)
    return generate_py_sync_method(method, model)


def generate_py_sync_method(method: Method, model: Model) -> Optional[str]:
    if synchronous_method_return_marshal(method) is None:
        return None
    if any(build_sync_param(param) is None for param in method.input_parameters):
        return None

    params = [facade_param(param, model) for param in method.input_parameters]
    names = ", ".join(facade_argument(param, model) for param in method.input_parameters)

    call = f"self._impl.{method.name}({names})"
    if method.return_value is not None:
        call = wrap_result(call, method.return_value.type, model)

    ret = "None" if method.return_value is None else pyi_type(method.return_value.type, model)

    return f"""    def {method.name}({facade_signature(method, params)}) -> {ret}:
        return {call}"""


def facade_signature(method: Method, params: List[str]) -> str:
    return ", ".join(["self"] + (method.custom_facade_params or params))


def facade_argument(param, model: Model) -> str:
    if resolve_input_object_type(param.type, model) is not None:
        return f"_unwrap({param.name})"
    return param.name


def facade_param(param, model: Model) -> str:
    annotation = pyi_type(param.type, model)
    if param.nullable:
        return f"{param.name}: Optional[{annotation}] = None"
    return f"{param.name}: {annotation}"


def generate_py_async_method(method: Method, model: Model) -> Optional[str]:
    if method.is_property_accessor:
        return None

    parts = build_facade_async_parts(method, model)
    if parts is None:
        return None
    signature, args = parts

    call = f"_invoke(self._impl.{method.name}, ({args}))"
    if method.return_value is not None:
        call = wrap_result(call, method.return_value.type, model)

    ret = "None" if method.return_value is None else pyi_type(method.return_value.type, model)

    return f"""    def {method.name}({signature}) -> {ret}:
        return {call}"""


def build_facade_async_parts(method: Method, model: Model) -> Optional[Tuple[str, str]]:
    params = []
    args = []
    for param in method.input_parameters:
        if param.type.name == "Gio.Cancellable":
            continue
        if build_async_param(param) is None:
            return None
        options = resolve_options_type(param.type, model)
        if options is not None:
            params.append("**kwargs")
            args.append(make_options_expr(options))
        elif resolve_input_object_type(param.type, model) is not None:
            params.append(f"{param.name}: Optional[{pyi_type(param.type, model)}] = None")
            args.append(f"_unwrap({param.name})")
        else:
            params.append(facade_param(param, model))
            args.append(param.name)

    if method.return_value is not None:
        if build_return_marshal(method.return_value.type, model) is None:
            return None

    return facade_signature(method, params), "".join(arg + ", " for arg in args)


def make_options_expr(options: ObjectType) -> str:
    return f"_make_options(_frida.{options.py_name}, kwargs, {build_option_selectors(options)})"


def build_option_selectors(options: ObjectType) -> str:
    selectors = {}
    otype = options
    while otype is not None:
        for method in otype.methods:
            if method.is_select_method:
                selectors[method.select_plural_noun] = method.name
        otype = otype.parent
    return repr(selectors)


def wrap_result(call: str, type: Type, model: Model) -> str:
    if resolve_object_type(type, model) is not None:
        return f"_wrap({call})"
    if resolve_list_type(type, model) is not None:
        return f"[_wrap(element) for element in {call}]"
    return call


def generate_extension_pyi(model: Model) -> str:
    lines = [
        "from typing import Any, Callable, Optional",
        "",
        "",
        "__version__: str",
        "",
        "",
    ]

    for member in model.error_domain.members:
        lines.append(f"class {member.js_name}Error(Exception): ...")
    lines.append("class OperationCancelledError(Exception): ...")

    for otype in model.regular_object_types:
        lines.append("")
        lines.append("")
        lines += generate_pyi_class(otype, model)

    lines.append("")
    lines.append("")
    for fn in module_functions(model):
        lines.append(f"def {fn.py_name}() -> Any: ...")
    lines.append("def _complete_request(request: Any, result: Any, error: Any) -> None: ...")
    lines.append("")

    return "\n".join(lines)


def generate_pyi_class(otype: ObjectType, model: Model) -> List[str]:
    parent = otype.parent
    header = f"class {otype.py_name}({parent.py_name}):" if parent is not None else f"class {otype.py_name}:"

    body = []

    primary = primary_constructor(otype)
    if primary is not None and not primary.throws:
        params = pyi_parameters(primary.input_parameters, model)
        if params is not None:
            signature = ", ".join(["self"] + params)
            body.append(f"    def __init__({signature}) -> None: ...")

    for ctor in otype.constructors:
        if ctor is primary:
            continue
        params = pyi_parameters(ctor.input_parameters, model)
        if params is None:
            continue
        body.append("    @staticmethod")
        body.append(f"    def {ctor.name}({', '.join(params)}) -> {otype.py_name}: ...")

    if otype.name == "Object":
        body.append("    def on(self, signal: str, callback: Callable[..., Any]) -> None: ...")
        body.append("    def off(self, signal: str, callback: Callable[..., Any]) -> None: ...")

    for method in otype.methods:
        name = property_name_from_accessor(method)
        if name is None or property_getter_marshal(method) is None:
            continue
        typing = pyi_type(method.return_value.type, model)
        body.append("    @property")
        body.append(f"    def {name}(self) -> {typing}: ...")
        set_method = next((m for m in otype.methods if m.name == f"set_{name}"), None)
        if set_method is not None and property_setter_supported(set_method):
            body.append(f"    @{name}.setter")
            body.append(f"    def {name}(self, value: {typing}) -> None: ...")

    for method in otype.methods:
        stub = pyi_method(method, model)
        if stub is not None:
            body.append(stub)

    if not body:
        body.append("    ...")

    return [header] + body


def pyi_method(method: Method, model: Model) -> Optional[str]:
    if method.is_property_accessor:
        return None

    if method.is_async:
        params = pyi_parameters(
            [p for p in method.input_parameters if p.type.name != "Gio.Cancellable"],
            model,
            builder=build_async_param,
        )
        if params is None:
            return None
        cancellable = next(
            (p for p in method.input_parameters if p.type.name == "Gio.Cancellable"),
            None,
        )
        cancellable_typing = pyi_type(cancellable.type, model) if cancellable is not None else "Any"
        signature = ", ".join(
            ["self"]
            + params
            + [
                "callback: Callable[[Any, Optional[Exception]], None]",
                f"cancellable: Optional[{cancellable_typing}] = ...",
            ]
        )
        return f"    def {method.name}({signature}) -> None: ..."

    if synchronous_method_return_marshal(method) is None:
        return None
    params = pyi_parameters(method.input_parameters, model, builder=build_sync_param)
    if params is None:
        return None
    signature = ", ".join(["self"] + params)
    ret = "None" if method.return_value is None else pyi_type(method.return_value.type, model)
    return f"    def {method.name}({signature}) -> {ret}: ..."


def pyi_parameters(params, model: Model, builder=None) -> Optional[List[str]]:
    result = []
    for param in params:
        if builder is not None and builder(param) is None:
            return None
        annotation = pyi_type(param.type, model)
        result.append(f"{param.name}: {annotation}")
    return result


def pyi_type(type: Type, model: Model) -> str:
    name = type.name
    if name == "utf8":
        return "str"
    if name == "utf8[]":
        return "list[str]"
    if name == "gboolean":
        return "bool"
    if name in {
        "gint8",
        "gint16",
        "gint",
        "gint32",
        "gint64",
        "guint8",
        "guint16",
        "guint",
        "guint32",
        "guint64",
        "gsize",
        "gssize",
    }:
        return "int"
    if name in {"gfloat", "gdouble"}:
        return "float"
    if name == "GLib.Bytes":
        return "bytes"
    if name == "GLib.HashTable":
        return "dict"
    if name == "GLib.Variant":
        return "Any"
    if name == "Gio.TlsCertificate":
        return "str"
    if resolve_enumeration(type, model) is not None:
        return "str"
    obj = resolve_object_type(type, model)
    if obj is not None:
        return f'"{obj.py_name}"'
    list_type = resolve_list_type(type, model)
    if list_type is not None:
        _, get = list_accessors(list_type)
        return f"list[{pyi_type(get.return_value.type, model)}]"
    return "Any"


def generate_extension_c(model: Model) -> str:
    code = generate_includes()
    code += CODEGEN_MACROS_H
    code += "\n" + CODEGEN_TYPEDEFS_H
    code += generate_object_type_typedefs(model)
    code += "\n\n" + CODEGEN_STRUCTS_H
    code += "\n" + generate_object_type_structs(model)
    code += "\n" + generate_prototypes(model)
    code += "\n" + generate_shared_globals(model)
    code += "\n" + CODEGEN_GOBJECT_GLOBALS
    code += "\n" + generate_object_type_method_definitions(model)
    code += "\n" + generate_object_type_getset_definitions(model)
    code += "\n" + generate_object_type_toplevel_definitions(model)
    for fn in module_functions(model):
        code += "\n" + read_asset(fn.asset).strip() + "\n"
    code += "\n" + generate_init_function(model)

    for otype in model.object_types.values():
        if otype.name == "Object":
            code += "\n" + CODEGEN_GOBJECT_METHODS
            continue
        if otype.is_frida_list:
            code += generate_list_conversion_functions(otype, model)
            continue

        if implementable_interface(otype):
            code += generate_interface_implementation(otype)

        code += generate_object_type_constructor(otype)
        for ctor in factory_constructors(otype):
            code += generate_factory_function(otype, ctor, build_constructor_marshals(ctor))

    return code


def implementable_interface(otype: ObjectType) -> bool:
    if not isinstance(otype, InterfaceObjectType) or not otype.has_abstract_base:
        return False
    return all(interface_method_supported(method, otype.model) for method in otype.methods)


def interface_type_marshalable(type: Type, model: Model) -> bool:
    return type.name == "utf8" or resolve_object_type(type, model) is not None


def interface_method_supported(method: Method, model: Model) -> bool:
    if not method.is_async:
        return False
    if method.return_value is None:
        return False
    if not interface_type_marshalable(method.return_value.type, model):
        return False
    return all(
        p.type.name == "Gio.Cancellable" or interface_type_marshalable(p.type, model) for p in method.input_parameters
    )


def interface_impl_names(otype: ObjectType):
    snake = to_snake_case(otype.name)
    return {
        "type": f"FridaPython{otype.name}",
        "prefix": f"frida_python_{snake}",
        "upper": snake.upper(),
        "cast": f"FRIDA_PYTHON_{snake.upper()}",
        "is": f"FRIDA_IS_PYTHON_{snake.upper()}",
    }


def generate_interface_implementation(otype: ObjectType) -> str:
    n = interface_impl_names(otype)
    methods = otype.methods

    forward = "".join(
        f"static void {n['prefix']}_{m.name} ({', '.join(m.param_ctypings)});\n"
        f"static {m.return_value.type.c} {n['prefix']}_{m.name}_finish ({', '.join(m.finish_param_ctypings)});\n"
        f"static void {n['prefix']}_complete_{m.name} (GTask * task, PyObject * value, PyObject * error);\n"
        for m in methods
    )

    iface_assignments = "\n".join(
        f"  iface->{m.name} = {n['prefix']}_{m.name};\n" f"  iface->{m.name}_finish = {n['prefix']}_{m.name}_finish;"
        for m in methods
    )

    method_functions = "".join(generate_interface_method(otype, m, n) for m in methods)

    return f"""
G_DECLARE_FINAL_TYPE ({n['type']}, {n['prefix']}, FRIDA, PYTHON_{n['upper']}, GObject)

struct _{n['type']}
{{
  GObject parent;
  PyObject * wrapper;
}};

static void {n['prefix']}_iface_init (gpointer g_iface, gpointer iface_data);
static void {n['prefix']}_dispose (GObject * object);
{forward}
G_DEFINE_TYPE_EXTENDED ({n['type']}, {n['prefix']}, G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE ({otype.get_type} (), {n['prefix']}_iface_init))

static {n['type']} *
{n['prefix']}_new (PyObject * wrapper)
{{
  {n['type']} * self = g_object_new ({n['prefix']}_get_type (), NULL);

  self->wrapper = wrapper;
  Py_IncRef (wrapper);

  return self;
}}

static void
{n['prefix']}_class_init ({n['type']}Class * klass)
{{
  G_OBJECT_CLASS (klass)->dispose = {n['prefix']}_dispose;
}}

static void
{n['prefix']}_iface_init (gpointer g_iface,
{" " * (len(n['prefix']) + len("_iface_init") + 2)}gpointer iface_data)
{{
  {otype.type_struct} * iface = g_iface;

{iface_assignments}
}}

static void
{n['prefix']}_init ({n['type']} * self)
{{
}}

static void
{n['prefix']}_dispose (GObject * object)
{{
  {n['type']} * self = {n['cast']} (object);

  if (self->wrapper != NULL)
  {{
    PyGILState_STATE gstate = PyGILState_Ensure ();
    Py_DecRef (self->wrapper);
    self->wrapper = NULL;
    PyGILState_Release (gstate);
  }}

  G_OBJECT_CLASS ({n['prefix']}_parent_class)->dispose (object);
}}

static int
{otype.c_symbol_prefix}_traverse (PyObject * self,
{" " * (len(otype.c_symbol_prefix) + len("_traverse ("))}visitproc visit,
{" " * (len(otype.c_symbol_prefix) + len("_traverse ("))}void * arg)
{{
  gpointer handle = PY_GOBJECT_HANDLE (self);

  /*
   * Only expose the wrapper back-reference to the cyclic GC while nothing but
   * this wrapper holds the handle. If frida-core still references it the extra
   * ref keeps the wrapper reachable, so hiding the edge prevents the GC from
   * collecting an interface implementation that is still in use.
   */
  if (handle != NULL && {n['is']} (handle) &&
      g_atomic_int_get (&((GObject *) handle)->ref_count) == 1)
    Py_VISIT ({n['cast']} (handle)->wrapper);

  return 0;
}}

static int
{otype.c_symbol_prefix}_clear (PyObject * self)
{{
  gpointer handle = PY_GOBJECT_HANDLE (self);

  if (handle != NULL && {n['is']} (handle))
    Py_CLEAR ({n['cast']} (handle)->wrapper);

  return 0;
}}
{method_functions}"""


def interface_arg_expr(param: Parameter, model: Model) -> str:
    if param.type.name == "utf8":
        return f"PyGObject_marshal_string ({param.name})"
    objtype = resolve_object_type(param.type, model)
    assert objtype is not None
    return f"PyGObject_marshal_object ({param.name}, {objtype.get_type} ())"


def generate_interface_method(otype: ObjectType, method: Method, n) -> str:
    model = otype.model
    params = [p for p in method.input_parameters if p.type.name != "Gio.Cancellable"]
    arg_exprs = ", ".join(interface_arg_expr(p, model) for p in params)
    fmt = "(" + "N" * len(params) + ")"
    build_args = f'Py_BuildValue ("{fmt}"{", " + arg_exprs if arg_exprs else ""})'

    cancellable = next(
        (p.name for p in method.input_parameters if p.type.name == "Gio.Cancellable"),
        "NULL",
    )

    return f"""
static void
{n['prefix']}_{method.name} ({', '.join(method.param_ctypings)})
{{
  {n['type']} * self = {n['cast']} ({method.cself_name});
  GTask * task;
  PyGILState_STATE gstate;
  PyObject * completion, * args, * outcome;

  task = g_task_new (self, {cancellable}, callback, user_data);

  gstate = PyGILState_Ensure ();

  completion = PyFrida_make_completion (task, {n['prefix']}_complete_{method.name});
  args = {build_args};
  outcome = PyObject_CallMethod (self->wrapper, "_frida_dispatch", "sNN", "{method.name}", args, completion);
  if (outcome != NULL)
    Py_DecRef (outcome);
  else
    PyErr_Print ();

  PyGILState_Release (gstate);
}}

static {method.return_value.type.c}
{n['prefix']}_{method.name}_finish ({', '.join(method.finish_param_ctypings)})
{{
  return g_task_propagate_pointer (G_TASK (result), error);
}}

static void
{n['prefix']}_complete_{method.name} (GTask * task,
{" " * (len(n['prefix']) + len(method.name) + len("_complete_") + 2)}PyObject * value,
{" " * (len(n['prefix']) + len(method.name) + len("_complete_") + 2)}PyObject * error)
{{
{generate_interface_complete_body(otype, method)}
}}
"""


def generate_interface_complete_body(otype: ObjectType, method: Method) -> str:
    if method.return_value.type.name == "utf8":
        return """\
  gchar * result;

  if (PyFrida_return_error (task, error))
    return;

  if (PyGObject_unmarshal_string (value, &result))
    g_task_return_pointer (task, result, g_free);
  else
    g_task_return_new_error (task, FRIDA_ERROR, FRIDA_ERROR_INVALID_ARGUMENT, "invalid result");"""

    return f"""\
  {method.return_value.type.c}result;

  if (PyFrida_return_error (task, error))
    return;

  result = (value != Py_None) ? g_object_ref (PY_GOBJECT_HANDLE (value)) : NULL;
  g_task_return_pointer (task, result, g_object_unref);"""


def generate_includes() -> str:
    return """\
#include <frida-core.h>

#define PY_SSIZE_T_CLEAN

/*
 * Don't propagate _DEBUG state to pyconfig as it incorrectly attempts to load
 * debug libraries that don't normally ship with Python (e.g. 2.x). Debuggers
 * wishing to spelunk the Python core can override this workaround by defining
 * _FRIDA_ENABLE_PYDEBUG.
 */
#if defined (_DEBUG) && !defined (_FRIDA_ENABLE_PYDEBUG)
# undef _DEBUG
# include <pyconfig.h>
# define _DEBUG
#else
# include <pyconfig.h>
#endif

#include <Python.h>

"""


def generate_prototypes(model: Model) -> str:
    prototypes = []

    for otype in model.regular_object_types:
        otype_cprefix = otype.c_symbol_prefix

        if otype.name == "Object":
            prototypes += [
                "",
                CODEGEN_GOBJECT_PROTOTYPES.rstrip(),
            ]
        else:
            prototypes += [
                "",
                f"static int {otype_cprefix}_init ({otype_cprefix} * self, PyObject * args, PyObject * kw);",
            ]
            for ctor in factory_constructors(otype):
                prototypes.append(
                    f"static PyObject * {otype_cprefix}_{ctor.name} (PyObject * self, PyObject * args, PyObject * kw);"
                )
            if implementable_interface(otype):
                prototypes += [
                    f"static int {otype_cprefix}_traverse (PyObject * self, visitproc visit, void * arg);",
                    f"static int {otype_cprefix}_clear (PyObject * self);",
                ]

    for otype in model.object_types.values():
        if otype.is_frida_list:
            prototypes += [
                "",
                f"static PyObject * {otype.c_symbol_prefix}_to_value ({otype.c_type} * self);",
            ]

    for fn in module_functions(model):
        prototypes += [
            "",
            f"static PyObject * {fn.c_symbol} (PyObject * module, PyObject * args);",
        ]

    return "\n".join(prototypes)


def module_functions(model: Model) -> List:
    result = []
    for otype in model.object_types.values():
        custom_code = otype.custom_code
        if custom_code is None or custom_code.module_functions is None:
            continue
        result += custom_code.module_functions
    return result


def generate_shared_globals(model: Model) -> str:
    method_entries = ['  { "_complete_request", PyFrida_complete_request, METH_VARARGS, NULL },']
    for fn in module_functions(model):
        method_entries.append(f'  {{ "{fn.py_name}", {fn.c_symbol}, METH_NOARGS, NULL }},')

    return "\n".join(
        [
            "",
            "static PyMethodDef PyFrida_functions[] =",
            "{",
            *method_entries,
            "  { NULL }",
            "};",
            'static struct PyModuleDef PyFrida_moduledef = { PyModuleDef_HEAD_INIT, "_frida", "Frida", -1, PyFrida_functions, };',
            "",
            "static initproc PyGObject_tp_init;",
            "static destructor PyGObject_tp_dealloc;",
        ]
    )


def generate_object_type_toplevel_definitions(model: Model) -> str:
    defs = []

    for otype in model.regular_object_types:
        cprefix = otype.c_symbol_prefix

        if otype.name == "Object":
            defs.append(f"""PYFRIDA_DEFINE_BASETYPE ("_frida.{otype.py_name}", {otype.py_name}, g_object_unref,
  {{ Py_tp_doc, "{otype.name}" }},
  {{ Py_tp_init, {cprefix}_init }},
  {{ Py_tp_dealloc, {cprefix}_dealloc }},
  {{ Py_tp_methods, {cprefix}_methods }},
  {{ Py_tp_getset, {cprefix}_getsets }},
);""")
            continue

        parent = otype.parent
        parent_name = parent.py_name if parent is not None else "GObject"

        if implementable_interface(otype):
            defs.append(
                f"""PYFRIDA_DEFINE_GC_TYPE ("_frida.{otype.py_name}", {otype.py_name}, {parent_name}, g_object_unref,
  {{ Py_tp_doc, "{otype.name}" }},
  {{ Py_tp_init, {cprefix}_init }},
  {{ Py_tp_dealloc, PyGObject_gc_dealloc }},
  {{ Py_tp_traverse, {cprefix}_traverse }},
  {{ Py_tp_clear, {cprefix}_clear }},
  {{ Py_tp_methods, {cprefix}_methods }},
  {{ Py_tp_getset, {cprefix}_getsets }},
);"""
            )
            continue

        defs.append(f"""PYFRIDA_DEFINE_TYPE ("_frida.{otype.py_name}", {otype.py_name}, {parent_name}, g_object_unref,
  {{ Py_tp_doc, "{otype.name}" }},
  {{ Py_tp_init, {cprefix}_init }},
  {{ Py_tp_methods, {cprefix}_methods }},
  {{ Py_tp_getset, {cprefix}_getsets }},
);""")

    return "\n\n".join(defs)


def generate_object_type_method_definitions(model: Model) -> str:
    return "\n\n".join(generate_object_type_methods(otype) for otype in model.regular_object_types)


def generate_object_type_methods(otype: ObjectType) -> str:
    functions = []
    entries = []

    if otype.name == "Object":
        entries.append('  { "on", (PyCFunction) PyGObject_on, METH_VARARGS, "Add a signal handler." },')
        entries.append('  { "off", (PyCFunction) PyGObject_off, METH_VARARGS, "Remove a signal handler." },')

    for method in otype.methods:
        emitted = generate_method(otype, method)
        if emitted is None:
            continue
        function, entry = emitted
        functions.append(function)
        entries.append(entry)

    for ctor in factory_constructors(otype):
        entries.append(factory_method_entry(otype, ctor))

    return f"""{"".join(functions)}
static PyMethodDef {otype.c_symbol_prefix}_methods[] =
{{
{chr(10).join(entries)}
  {{ NULL }}
}};"""


def generate_method(otype: ObjectType, method: Method) -> Optional[Tuple[str, str]]:
    if method.is_async:
        return generate_async_method(otype, method)

    marshal = synchronous_method_return_marshal(method)
    if marshal is None:
        return None
    params = []
    for param in method.input_parameters:
        sync_param = build_sync_param(param)
        if sync_param is None:
            return None
        params.append(sync_param)
    function = generate_synchronous_method(otype, method, marshal, params)
    flags = "METH_VARARGS" if params else "METH_NOARGS"
    entry = f'  {{ "{method.name}", (PyCFunction) {otype.c_symbol_prefix}_{method.name}, {flags}, NULL }},'
    return function, entry


def generate_async_method(otype: ObjectType, method: Method) -> Optional[Tuple[str, str]]:
    if method.is_property_accessor:
        return None

    params = []
    for param in method.input_parameters:
        if param.type.name == "Gio.Cancellable":
            continue
        async_param = build_async_param(param)
        if async_param is None:
            return None
        params.append(async_param)

    marshal = ""
    if method.return_value is not None:
        return_marshal = build_return_marshal(method.return_value.type, method.object_type.model)
        if return_marshal is None:
            return None
        marshal = return_marshal

    cprefix = otype.c_symbol_prefix
    op = method.operation_type_name
    entry_fn = f"{cprefix}_{method.name}"
    begin_fn = f"{cprefix}_{method.name}_begin"
    end_fn = f"{cprefix}_{method.name}_end"
    has_cancellable = any(p.type.name == "Gio.Cancellable" for p in method.input_parameters)

    return (
        generate_async_operation_struct(otype, op, params, has_cancellable)
        + generate_async_forward_declarations(begin_fn, end_fn)
        + generate_async_entry(otype, method, params, has_cancellable, op, entry_fn, begin_fn)
        + generate_async_begin(method, params, has_cancellable, op, begin_fn, end_fn)
        + generate_async_end(method, marshal, op, end_fn),
        f'  {{ "{method.name}", (PyCFunction) {entry_fn}, METH_VARARGS, NULL }},',
    )


def generate_async_operation_struct(
    otype: ObjectType, op: str, params: List["AsyncParam"], has_cancellable: bool
) -> str:
    fields = [f"  {p.field}" for p in params]
    if has_cancellable:
        fields.append("  GCancellable * cancellable;")
    return f"""
typedef struct {{
  PyObject * callback;
  {otype.c_type} * handle;
{chr(10).join(fields)}
}} {op};
"""


def generate_async_forward_declarations(begin_fn: str, end_fn: str) -> str:
    return f"""
static gboolean {begin_fn} (gpointer user_data);
static void {end_fn} (GObject * source_object, GAsyncResult * res, gpointer user_data);
"""


def generate_async_entry(
    otype: ObjectType,
    method: Method,
    params: List["AsyncParam"],
    has_cancellable: bool,
    op: str,
    entry_fn: str,
    begin_fn: str,
) -> str:
    local_decls = "".join(f"  {p.local_decl}\n" for p in params)
    cancellable_decl = "  PyObject * cancellable = NULL;\n" if has_cancellable else ""

    fmt = "".join(p.fmt for p in params) + "O" + ("|O" if has_cancellable else "")
    parse_args = ", ".join(
        [f'"{fmt}"']
        + [arg for p in params for arg in p.parse_args]
        + ["&callback"]
        + (["&cancellable"] if has_cancellable else [])
    )

    validations = "".join(f"\n  {p.validate}\n" for p in params if p.validate is not None)
    stores = "".join(f"  {p.store}\n" for p in params)
    cancellable_store = ""
    if has_cancellable:
        cancellable_store = """  if (cancellable != NULL && cancellable != Py_None)
    operation->cancellable = (GCancellable *) g_object_ref (PY_GOBJECT_HANDLE (cancellable));
"""

    return f"""
static PyObject *
{entry_fn} ({otype.c_symbol_prefix} * self,
{" " * (len(entry_fn) + 2)}PyObject * args)
{{
{local_decls}  PyObject * callback;
{cancellable_decl}  {op} * operation;
  GSource * idle;

  if (!PyArg_ParseTuple (args, {parse_args}))
    return NULL;
{validations}
  operation = g_slice_new0 ({op});
  operation->callback = callback;
  Py_IncRef (callback);
  operation->handle = ({otype.c_type} *) g_object_ref (PY_GOBJECT_HANDLE (self));
{stores}{cancellable_store}
  idle = g_idle_source_new ();
  g_source_set_callback (idle, {begin_fn}, operation, NULL);
  g_source_attach (idle, frida_get_main_context ());
  g_source_unref (idle);

  PyFrida_RETURN_NONE;
}}
"""


def generate_async_begin(
    method: Method,
    params: List["AsyncParam"],
    has_cancellable: bool,
    op: str,
    begin_fn: str,
    end_fn: str,
) -> str:
    start_args = ", ".join(
        ["operation->handle"]
        + [p.start_arg for p in params]
        + (["operation->cancellable"] if has_cancellable else [])
        + [end_fn, "operation"]
    )
    return f"""
static gboolean
{begin_fn} (gpointer user_data)
{{
  {op} * operation = user_data;

  {method.c_identifier} ({start_args});

  return FALSE;
}}
"""


def generate_async_end(method: Method, marshal: str, op: str, end_fn: str) -> str:
    finish_args = ", ".join(["operation->handle", "res"] + (["&error"] if method.throws else []))
    error_decl = "\n  GError * error = NULL;" if method.throws else ""

    if method.return_value is None:
        finish_call = f"{method.finish_c_identifier} ({finish_args});"
        success = "    Py_IncRef (Py_None);\n    value = Py_None;"
    else:
        finish_call = f"{method.return_value.type.c} retval = {method.finish_c_identifier} ({finish_args});"
        release_retval = ""
        destroy = method.return_value.destroy_func
        if destroy is not None:
            release_retval = f"\n    {generate_destruction_code('retval', destroy)}"
        success = f"    value = {marshal.format(value='retval')};{release_retval}"

    error_delivery = ""
    if method.throws:
        error_delivery = """  if (error != NULL)
  {
    PyObject * exception = PyFrida_marshal_error (error);
    PyFrida_deliver (operation->callback, Py_None, exception);
    Py_DecRef (exception);
  }
  else
"""

    cleanups = "".join(f"  {p.free}\n" for p in method_async_params(method) if p.free)
    cancellable_cleanup = (
        "  g_clear_object (&operation->cancellable);\n"
        if any(p.type.name == "Gio.Cancellable" for p in method.input_parameters)
        else ""
    )

    return f"""
static void
{end_fn} (GObject * source_object,
{" " * (len(end_fn) + 2)}GAsyncResult * res,
{" " * (len(end_fn) + 2)}gpointer user_data)
{{
  {op} * operation = user_data;{error_decl}
  PyGILState_STATE gstate;
  PyObject * value;

  {finish_call}

  gstate = PyGILState_Ensure ();

{error_delivery}  {{
{success}
    PyFrida_deliver (operation->callback, value, Py_None);
    Py_DecRef (value);
  }}

  Py_DecRef (operation->callback);
  g_object_unref (operation->handle);
{cleanups}{cancellable_cleanup}  g_slice_free ({op}, operation);

  PyGILState_Release (gstate);
}}
"""


def method_async_params(method: Method) -> List["AsyncParam"]:
    result = []
    for param in method.input_parameters:
        if param.type.name == "Gio.Cancellable":
            continue
        async_param = build_async_param(param)
        if async_param is not None:
            result.append(async_param)
    return result


def build_async_param(param: Parameter) -> Optional["AsyncParam"]:
    name = param.name
    type_name = param.type.name

    if type_name == "utf8":
        fmt = "z" if param.nullable else "s"
        return AsyncParam(
            field=f"gchar * {name};",
            local_decl=f"const char * {name};",
            fmt=fmt,
            parse_args=[f"&{name}"],
            store=f"operation->{name} = g_strdup ({name});",
            free=f"g_free (operation->{name});",
            start_arg=f"operation->{name}",
        )

    numeric = PYARG_NUMERIC_FORMATS.get(type_name)
    if numeric is not None:
        fmt, c_type = numeric
        return AsyncParam(
            field=f"{param.type.c} {name};",
            local_decl=f"{c_type} {name};",
            fmt=fmt,
            parse_args=[f"&{name}"],
            store=f"operation->{name} = {name};",
            start_arg=f"operation->{name}",
        )

    enum = resolve_enumeration(param.type, param.object_type.model)
    if enum is not None:
        return AsyncParam(
            field=f"{param.type.c} {name};",
            local_decl=f"const char * {name}_value;\n  {param.type.c} {name};",
            fmt="s",
            parse_args=[f"&{name}_value"],
            validate=f"""if (!PyGObject_unmarshal_enum ({name}_value, {enum.get_type} (), &{name}))
    return NULL;""",
            store=f"operation->{name} = {name};",
            start_arg=f"operation->{name}",
        )

    if type_name == "GLib.Bytes":
        return AsyncParam(
            field=f"GBytes * {name};",
            local_decl=f"const char * {name}_data;\n  Py_ssize_t {name}_size;",
            fmt="z#",
            parse_args=[f"&{name}_data", f"&{name}_size"],
            store=f"""if ({name}_data != NULL)
    operation->{name} = g_bytes_new ({name}_data, {name}_size);""",
            free=f"g_clear_pointer (&operation->{name}, g_bytes_unref);",
            start_arg=f"operation->{name}",
        )

    if type_name == "GLib.Variant":
        return AsyncParam(
            field=f"GVariant * {name};",
            local_decl=f"PyObject * {name}_obj;\n  GVariant * {name};",
            fmt="O",
            parse_args=[f"&{name}_obj"],
            validate=f"""if (!PyGObject_unmarshal_variant ({name}_obj, &{name}))
    return NULL;""",
            store=f"operation->{name} = {name};",
            free=f"g_clear_pointer (&operation->{name}, g_variant_unref);",
            start_arg=f"operation->{name}",
        )

    if resolve_input_object_type(param.type, param.object_type.model) is not None:
        return AsyncParam(
            field=f"{param.type.c} {name};",
            local_decl=f"PyObject * {name}_obj;",
            fmt="O",
            parse_args=[f"&{name}_obj"],
            store=f"""if ({name}_obj != Py_None)
    operation->{name} = ({param.type.c}) g_object_ref (PY_GOBJECT_HANDLE ({name}_obj));""",
            free=f"g_clear_object (&operation->{name});",
            start_arg=f"operation->{name}",
        )

    return None


def resolve_input_object_type(type: Type, model: Model) -> Optional[ObjectType]:
    tokens = type.name.split(".", maxsplit=1)
    if len(tokens) != 2:
        return None
    otype = model.object_types.get(tokens[1])
    if otype is None or otype.is_frida_list:
        return None
    return otype


class AsyncParam:
    def __init__(
        self,
        field: str,
        local_decl: str,
        fmt: str,
        parse_args: List[str],
        store: str,
        start_arg: str,
        validate: Optional[str] = None,
        free: str = "",
    ):
        self.field = field
        self.local_decl = local_decl
        self.fmt = fmt
        self.parse_args = parse_args
        self.store = store
        self.start_arg = start_arg
        self.validate = validate
        self.free = free


def generate_synchronous_method(otype: ObjectType, method: Method, marshal: str, params: List["SyncParam"]) -> str:
    indent = " " * (len(otype.c_symbol_prefix) + len(method.name) + 3)
    handle = f"({otype.c_type} *) PY_GOBJECT_HANDLE (self)"
    returns_strv = method.return_value is not None and method.return_value.type.name == "utf8[]"
    call_arg_list = [handle] + [p.call_arg for p in params]
    if returns_strv:
        call_arg_list.append("&retval_length")
    if method.throws:
        call_arg_list.append("&error")
    call = f"{method.c_identifier} ({', '.join(call_arg_list)})"

    if not params and not method.throws and not returns_strv:
        if method.return_value is None:
            body = f"  {call};\n\n  PyFrida_RETURN_NONE;"
        else:
            body = f"  return {marshal.format(value=call)};"
        return f"""
static PyObject *
{otype.c_symbol_prefix}_{method.name} ({otype.c_symbol_prefix} * self,
{indent}PyObject * args)
{{
{body}
}}
"""

    decls = ["  PyObject * result;"]
    if method.throws:
        decls.append("  GError * error = NULL;")
    if returns_strv:
        decls.append("  gint retval_length;")
    for p in params:
        decls += [f"  {line}" for line in p.decl.splitlines()]

    parse = ""
    if params:
        fmt = "".join(p.fmt for p in params)
        parse_args = ", ".join(["args", f'"{fmt}"'] + [a for p in params for a in p.parse_args])
        parse = f"""
  if (!PyArg_ParseTuple ({parse_args}))
    return NULL;
"""

    pre = "".join(f"\n  {p.pre}\n" for p in params if p.pre)

    if method.return_value is None:
        invocation = f"  {call};"
        success = "  Py_IncRef (Py_None);\n  result = Py_None;"
    elif returns_strv:
        invocation = f"  gchar ** retval = {call};"
        success = "  result = PyGObject_marshal_strv (retval, retval_length);\n  g_strfreev (retval);"
    else:
        invocation = f"  {method.return_value.type.c} retval = {call};"
        release = ""
        if method.return_value.destroy_func is not None:
            release = f"\n  {generate_destruction_code('retval', method.return_value.destroy_func)}"
        success = f"  result = {marshal.format(value='retval')};{release}"

    cleanups = "".join(f"  {p.cleanup}\n" for p in params if p.cleanup)

    if method.throws:
        tail = f"""
  if (error != NULL)
  {{
    result = PyFrida_raise (error);
    goto beach;
  }}

{success}

beach:
{cleanups}  return result;"""
    else:
        tail = f"""{success}

{cleanups}  return result;"""

    return f"""
static PyObject *
{otype.c_symbol_prefix}_{method.name} ({otype.c_symbol_prefix} * self,
{indent}PyObject * args)
{{
{chr(10).join(decls)}
{parse}{pre}
{invocation}
{tail}
}}
"""


def synchronous_method_return_marshal(method: Method) -> Optional[str]:
    if method.is_async or method.is_property_accessor:
        return None
    if method.return_value is None:
        return ""
    if method.return_value.type.name == "utf8[]":
        return "PyGObject_marshal_strv"
    return build_return_marshal(method.return_value.type, method.object_type.model)


class SyncParam:
    def __init__(
        self,
        decl: str,
        fmt: str,
        parse_args: List[str],
        call_arg: str,
        pre: str = "",
        cleanup: str = "",
    ):
        self.decl = decl
        self.fmt = fmt
        self.parse_args = parse_args
        self.call_arg = call_arg
        self.pre = pre
        self.cleanup = cleanup


def build_sync_param(param: Parameter) -> Optional["SyncParam"]:
    name = param.name
    type_name = param.type.name

    if type_name == "utf8":
        return SyncParam(
            decl=f"const char * {name};",
            fmt="z" if param.nullable else "s",
            parse_args=[f"&{name}"],
            call_arg=name,
        )

    numeric = PYARG_NUMERIC_FORMATS.get(type_name)
    if numeric is not None:
        fmt, c_type = numeric
        return SyncParam(
            decl=f"{c_type} {name};",
            fmt=fmt,
            parse_args=[f"&{name}"],
            call_arg=name,
        )

    enum = resolve_enumeration(param.type, param.object_type.model)
    if enum is not None:
        return SyncParam(
            decl=f"const char * {name}_value;\n{param.type.c} {name};",
            fmt="s",
            parse_args=[f"&{name}_value"],
            pre=f"""if (!PyGObject_unmarshal_enum ({name}_value, {enum.get_type} (), &{name}))
    return NULL;""",
            call_arg=name,
        )

    if type_name == "GLib.Bytes":
        return SyncParam(
            decl=f"const char * {name}_data;\nPy_ssize_t {name}_size;\nGBytes * {name} = NULL;",
            fmt="z#",
            parse_args=[f"&{name}_data", f"&{name}_size"],
            pre=f"if ({name}_data != NULL)\n    {name} = g_bytes_new ({name}_data, {name}_size);",
            call_arg=name,
            cleanup=f"g_clear_pointer (&{name}, g_bytes_unref);",
        )

    if type_name == "GLib.Variant":
        return SyncParam(
            decl=f"PyObject * {name}_obj;\nGVariant * {name} = NULL;",
            fmt="O",
            parse_args=[f"&{name}_obj"],
            pre=f"""if (!PyGObject_unmarshal_variant ({name}_obj, &{name}))
    return NULL;""",
            call_arg=name,
            cleanup=f"g_clear_pointer (&{name}, g_variant_unref);",
        )

    if resolve_input_object_type(param.type, param.object_type.model) is not None:
        return SyncParam(
            decl=f"PyObject * {name}_obj;\n{param.type.c} {name} = NULL;",
            fmt="O",
            parse_args=[f"&{name}_obj"],
            pre=f"""if ({name}_obj != Py_None)
    {name} = ({param.type.c}) PY_GOBJECT_HANDLE ({name}_obj);""",
            call_arg=name,
        )

    return None


def generate_object_type_getset_definitions(model: Model) -> str:
    return "\n\n".join(generate_object_type_getset(otype) for otype in model.regular_object_types)


def generate_object_type_getset(otype: ObjectType) -> str:
    accessors = [
        (name, method, marshal)
        for method in otype.methods
        for name in [property_name_from_accessor(method)]
        for marshal in [property_getter_marshal(method)]
        if name is not None and marshal is not None
    ]
    set_methods = {
        method.name[len("set_") :]: method
        for method in otype.methods
        if method.is_property_accessor and method.name.startswith("set_")
    }

    functions = []
    entries = []
    for name, method, marshal in accessors:
        functions.append(generate_property_getter(otype, name, method, marshal))

        setter = "NULL"
        set_method = set_methods.get(name)
        if set_method is not None:
            setter_fn = generate_property_setter(otype, name, set_method)
            if setter_fn is not None:
                functions.append(setter_fn)
                setter = f"(setter) {otype.c_symbol_prefix}_set_{name}"

        entries.append(f'  {{ "{name}", (getter) {otype.c_symbol_prefix}_get_{name}, {setter}, NULL, NULL }},')

    return f"""{"".join(functions)}
static PyGetSetDef {otype.c_symbol_prefix}_getsets[] =
{{
{chr(10).join(entries)}
  {{ NULL }}
}};"""


def generate_property_setter(otype: ObjectType, name: str, set_method: Method) -> Optional[str]:
    handle = f"({otype.c_type} *) PY_GOBJECT_HANDLE (self)"
    indent = " " * (len(otype.c_symbol_prefix) + len(name) + len("_set_ (") + 1)
    param = set_method.input_parameters[0]

    if param.type.name == "utf8[]":
        return f"""
static int
{otype.c_symbol_prefix}_set_{name} ({otype.c_symbol_prefix} * self,
{indent}PyObject * value,
{indent}void * closure)
{{
  gchar ** strv;
  gint length;

  if (!PyGObject_unmarshal_strv (value, &strv, &length))
    return -1;

  {set_method.c_identifier} ({handle}, strv, length);

  g_strfreev (strv);

  return 0;
}}
"""

    body = build_setter_body(param, lambda arg: f"{set_method.c_identifier} ({handle}, {arg});")
    if body is None:
        return None

    return f"""
static int
{otype.c_symbol_prefix}_set_{name} ({otype.c_symbol_prefix} * self,
{indent}PyObject * value,
{indent}void * closure)
{{
{body}
  return 0;
}}
"""


def property_setter_supported(set_method: Method) -> bool:
    param = set_method.input_parameters[0]
    if param.type.name == "utf8[]":
        return True
    return build_setter_body(param, lambda arg: "") is not None


def build_setter_body(param: Parameter, set_call) -> Optional[str]:
    type_name = param.type.name

    if type_name == "utf8":
        return f"""  PyObject * bytes = NULL;
  const char * str = NULL;

  if (value != Py_None)
  {{
    bytes = PyUnicode_AsUTF8String (value);
    if (bytes == NULL)
      return -1;
    str = PyBytes_AsString (bytes);
  }}

  {set_call("str")}

  Py_XDECREF (bytes);
"""

    if type_name == "gboolean":
        return f"  {set_call('PyObject_IsTrue (value)')}\n"

    if type_name in {"gint8", "gint16", "gint", "gint32", "guint8", "guint16", "guint", "guint32"}:
        return f"""  long number = PyLong_AsLong (value);
  if (number == -1 && PyErr_Occurred () != NULL)
    return -1;

  {set_call("number")}
"""

    if type_name in {"gint64", "guint64"}:
        return f"""  long long number = PyLong_AsLongLong (value);
  if (number == -1 && PyErr_Occurred () != NULL)
    return -1;

  {set_call("number")}
"""

    enum = resolve_enumeration(param.type, param.object_type.model)
    if enum is not None:
        return f"""  PyObject * bytes;
  {param.type.c} enum_value;
  gboolean valid;

  bytes = PyUnicode_AsUTF8String (value);
  if (bytes == NULL)
    return -1;

  valid = PyGObject_unmarshal_enum (PyBytes_AsString (bytes), {enum.get_type} (), &enum_value);
  Py_DecRef (bytes);
  if (!valid)
    return -1;

  {set_call("enum_value")}
"""

    if type_name == "GLib.Bytes":
        return f"""  char * data = NULL;
  Py_ssize_t size = 0;
  GBytes * bytes = NULL;

  if (value != Py_None)
  {{
    if (PyBytes_AsStringAndSize (value, &data, &size) < 0)
      return -1;
    bytes = g_bytes_new (data, size);
  }}

  {set_call("bytes")}

  g_clear_pointer (&bytes, g_bytes_unref);
"""

    if type_name == "GLib.HashTable":
        return f"""  GHashTable * dict;

  if (!PyGObject_unmarshal_vardict (value, &dict))
    return -1;

  {set_call("dict")}

  g_hash_table_unref (dict);
"""

    if type_name == "Gio.TlsCertificate":
        return f"""  GTlsCertificate * certificate;

  if (!PyGObject_unmarshal_certificate (value, &certificate))
    return -1;

  {set_call("certificate")}

  g_clear_object (&certificate);
"""

    if resolve_input_object_type(param.type, param.object_type.model) is not None:
        handle = f"({param.type.c}) ((value != Py_None) ? PY_GOBJECT_HANDLE (value) : NULL)"
        return f"  {set_call(handle)}\n"

    return None


def generate_property_getter(otype: ObjectType, name: str, method: Method, marshal: str) -> str:
    handle = f"({otype.c_type} *) PY_GOBJECT_HANDLE (self)"
    indent = " " * (len(otype.c_symbol_prefix) + len(name) + len("_get_ (") + 1)

    if method.return_value.type.name == "utf8[]":
        return f"""
static PyObject *
{otype.c_symbol_prefix}_get_{name} ({otype.c_symbol_prefix} * self,
{indent}void * closure)
{{
  gint length;
  gchar ** value = {method.c_identifier} ({handle}, &length);

  return PyGObject_marshal_strv (value, length);
}}
"""

    value = f"{method.c_identifier} ({handle})"
    return f"""
static PyObject *
{otype.c_symbol_prefix}_get_{name} ({otype.c_symbol_prefix} * self,
{indent}void * closure)
{{
  return {marshal.format(value=value)};
}}
"""


def property_name_from_accessor(method: Method) -> Optional[str]:
    if not method.is_property_accessor:
        return None
    if method.name.startswith("get_"):
        return method.name[len("get_") :]
    if method.name.startswith("is_"):
        return method.name
    return None


def property_getter_marshal(method: Method) -> Optional[str]:
    if property_name_from_accessor(method) is None:
        return None
    if method.return_value is None:
        return None
    if method.return_value.type.name == "utf8[]":
        return "PyGObject_marshal_strv (...)"
    return build_return_marshal(method.return_value.type, method.object_type.model)


def build_return_marshal(type: Type, model: Model) -> Optional[str]:
    name = type.name

    if name == "utf8":
        return "PyGObject_marshal_string ({value})"
    if name == "gboolean":
        return "PyBool_FromLong ({value})"
    if name in {"gint8", "gint16", "gint", "gint32"}:
        return "PyLong_FromLong ({value})"
    if name in {"guint8", "guint16", "guint", "guint32"}:
        return "PyLong_FromUnsignedLong ({value})"
    if name == "gint64":
        return "PyLong_FromLongLong ({value})"
    if name == "guint64":
        return "PyLong_FromUnsignedLongLong ({value})"
    if name in {"gssize", "gsize"}:
        return "PyLong_FromSsize_t ({value})"

    if name == "GLib.Bytes":
        return "PyGObject_marshal_bytes ({value})"
    if name == "GLib.HashTable":
        return "PyGObject_marshal_vardict ({value})"
    if name == "GLib.Variant":
        return "PyGObject_marshal_variant ({value})"
    if name == "Gio.TlsCertificate":
        return "PyGObject_marshal_certificate ({value})"

    enum = resolve_enumeration(type, model)
    if enum is not None:
        return "PyGObject_marshal_enum ({value}, " + enum.get_type + " ())"

    obj = resolve_object_type(type, model)
    if obj is not None:
        return "PyGObject_marshal_object ({value}, " + obj.get_type + " ())"

    list_type = resolve_list_type(type, model)
    if list_type is not None:
        return f"{list_type.c_symbol_prefix}_to_value ({{value}})"

    return None


def resolve_object_type(type: Type, model: Model) -> Optional[ObjectType]:
    tokens = type.name.split(".", maxsplit=1)
    if len(tokens) != 2:
        return None
    otype = model.object_types.get(tokens[1])
    if otype is None or otype.is_frida_options or otype.is_frida_list:
        return None
    return otype


def resolve_options_type(type: Type, model: Model) -> Optional[ObjectType]:
    tokens = type.name.split(".", maxsplit=1)
    if len(tokens) != 2:
        return None
    otype = model.object_types.get(tokens[1])
    if otype is None or not otype.is_frida_options:
        return None
    return otype


def resolve_list_type(type: Type, model: Model) -> Optional[ObjectType]:
    tokens = type.name.split(".", maxsplit=1)
    if len(tokens) != 2:
        return None
    otype = model.object_types.get(tokens[1])
    if otype is None or not otype.is_frida_list:
        return None
    return otype


def list_accessors(otype: ObjectType) -> Tuple[Method, Method]:
    size = next(method for method in otype.methods if method.name == "size")
    get = next(method for method in otype.methods if method.name == "get")
    return size, get


def generate_list_conversion_functions(otype: ObjectType, model: Model) -> str:
    size, get = list_accessors(otype)
    element = get.return_value.type
    element_return_marshal = build_return_marshal(element, model)
    assert element_return_marshal is not None
    element_marshal = element_return_marshal.format(value="element")

    release = ""
    if get.return_value.destroy_func is not None:
        release = f"\n    {generate_destruction_code('element', get.return_value.destroy_func)}"

    return f"""
static PyObject *
{otype.c_symbol_prefix}_to_value ({otype.c_type} * self)
{{
  gint size, i;
  PyObject * result;

  if (self == NULL)
    PyFrida_RETURN_NONE;

  size = {size.c_identifier} (self);
  result = PyList_New (size);

  for (i = 0; i != size; i++)
  {{
    {element.c} element = {get.c_identifier} (self, i);
    PyList_SetItem (result, i, {element_marshal});{release}
  }}

  return result;
}}
"""


def generate_init_function(model: Model) -> str:
    registration_calls = [
        f"PYFRIDA_REGISTER_TYPE ({otype.py_name}, {otype.get_type} ());" for otype in model.regular_object_types
    ]
    subtype_registration_calls = "\n  ".join(registration_calls[1:])
    exception_registration = indent_c_code(generate_exception_registration(model.error_domain), 1, prologue="\n")

    return f"""
PyMODINIT_FUNC
PyInit__frida (void)
{{
  PyObject * module, * inspect;

  frida_init ();

  PyGObject_class_init ();

  module = PyModule_Create (&PyFrida_moduledef);

  PyModule_AddStringConstant (module, "__version__", frida_version_string ());

  inspect = PyImport_ImportModule ("inspect");
  inspect_getargspec = PyObject_GetAttrString (inspect, "getfullargspec");
  inspect_ismethod = PyObject_GetAttrString (inspect, "ismethod");
  Py_DecRef (inspect);

  {registration_calls[0]}
  PyGObject_tp_init = PyType_GetSlot ((PyTypeObject *) PYFRIDA_TYPE_OBJECT (GObject), Py_tp_init);
  PyGObject_tp_dealloc = PyType_GetSlot ((PyTypeObject *) PYFRIDA_TYPE_OBJECT (GObject), Py_tp_dealloc);

  {subtype_registration_calls}
{exception_registration}

  return module;
}}
"""


def generate_exception_registration(error_domain: Enumeration) -> str:
    lines = ["frida_exception_by_error_code = g_hash_table_new_full (NULL, NULL, NULL, PyFrida_object_decref);"]
    lines += [
        f'PYFRIDA_DECLARE_EXCEPTION ({member.c_identifier}, "{member.js_name}");' for member in error_domain.members
    ]
    lines += [
        'cancelled_exception = PyErr_NewException ("frida.OperationCancelledError", NULL, NULL);',
        "Py_IncRef (cancelled_exception);",
        'PyModule_AddObject (module, "OperationCancelledError", cancelled_exception);',
    ]
    return "\n".join(lines)


def generate_object_type_constructor(otype: ObjectType) -> str:
    custom_constructor = otype.custom_constructor
    if custom_constructor is not None:
        return "\n" + read_asset(custom_constructor)

    if implementable_interface(otype):
        return generate_interface_init(otype)

    ctor = primary_constructor(otype)

    if ctor is None:
        return generate_bare_init(otype)

    if ctor.throws:
        return generate_unconstructable_init(otype)

    marshals = build_constructor_marshals(ctor)
    if marshals is None:
        return generate_unconstructable_init(otype)

    return generate_constructor_init(otype, ctor, marshals)


def primary_constructor(otype: ObjectType) -> Optional[Procedure]:
    if not otype.constructors:
        return None
    return next((c for c in otype.constructors if c.name == "new"), otype.constructors[0])


def factory_constructors(otype: ObjectType) -> List[Procedure]:
    primary = primary_constructor(otype)
    return [
        ctor
        for ctor in otype.constructors
        if ctor is not primary and not ctor.is_async and build_constructor_marshals(ctor) is not None
    ]


def build_constructor_marshals(ctor: Procedure) -> Optional[List["ParamMarshal"]]:
    marshals = []
    for param in ctor.input_parameters:
        marshal = build_param_marshal(param)
        if marshal is None:
            return None
        marshal.optional = param.optional
        marshals.append(marshal)
    return marshals


def generate_interface_init(otype: ObjectType) -> str:
    n = interface_impl_names(otype)
    indent = constructor_indent(otype)
    return f"""
static int
{otype.c_symbol_prefix}_init ({otype.c_symbol_prefix} * self,
{indent}PyObject * args,
{indent}PyObject * kw)
{{
  PyObject * wrapper;

  if (PyGObject_tp_init ((PyObject *) self, args, kw) < 0)
    return -1;

  if (!PyArg_ParseTuple (args, "O", &wrapper))
    return -1;

  PyGObject_take_handle ((PyGObject *) self, {n['prefix']}_new (wrapper), PYFRIDA_TYPE ({otype.py_name}));

  return 0;
}}
"""


def generate_bare_init(otype: ObjectType) -> str:
    indent = constructor_indent(otype)
    return f"""
static int
{otype.c_symbol_prefix}_init ({otype.c_symbol_prefix} * self,
{indent}PyObject * args,
{indent}PyObject * kw)
{{
  return PyGObject_tp_init ((PyObject *) self, args, kw);
}}
"""


def generate_unconstructable_init(otype: ObjectType) -> str:
    indent = constructor_indent(otype)
    return f"""
static int
{otype.c_symbol_prefix}_init ({otype.c_symbol_prefix} * self,
{indent}PyObject * args,
{indent}PyObject * kw)
{{
  if (PyGObject_tp_init ((PyObject *) self, args, kw) < 0)
    return -1;

  PyErr_SetString (PyExc_NotImplementedError, "{otype.py_name} cannot be constructed yet");
  return -1;
}}
"""


def generate_constructor_init(otype: ObjectType, ctor: Procedure, marshals: List["ParamMarshal"]) -> str:
    indent = constructor_indent(otype)

    decls = []
    for m in marshals:
        decls += m.decls
    if any(m.cleanup for m in marshals) and constructor_needs_beach(marshals):
        decls.insert(0, "int result = -1;")
    decls.append(f"{otype.c_type} * handle;")
    decls_block = indent_c_code("\n".join(decls), 1, prologue="\n")

    keyword_decl = ""
    parse_block = ""
    if marshals:
        keywords = ", ".join([f'"{m.keyword}"' for m in marshals] + ["NULL"])
        keyword_decl = f"\n  static char * keywords[] = {{ {keywords} }};"
        parse_block = "\n" + generate_argument_parse(marshals)

    post_block = ""
    for m in marshals:
        if m.post is not None:
            post_block += indent_c_code(m.post, 1, prologue="\n") + "\n"

    call_args = ", ".join(m.call_arg for m in marshals)
    constructor_call = f"handle = ({otype.c_type} *) {ctor.c_identifier} ({call_args});"
    take_handle = (
        "PyGObject_take_handle ((PyGObject *) self, g_steal_pointer (&handle), " f"PYFRIDA_TYPE ({otype.py_name}));"
    )

    return f"""
static int
{otype.c_symbol_prefix}_init ({otype.c_symbol_prefix} * self,
{indent}PyObject * args,
{indent}PyObject * kw)
{{{keyword_decl}{decls_block}

  if (PyGObject_tp_init ((PyObject *) self, args, kw) < 0)
    return -1;
{parse_block}{post_block}
  {constructor_call}

  {take_handle}

{generate_constructor_tail(marshals)}"""


def factory_method_entry(otype: ObjectType, ctor: Procedure) -> str:
    return (
        f'  {{ "{ctor.name}", (PyCFunction) {otype.c_symbol_prefix}_{ctor.name}, '
        "METH_VARARGS | METH_KEYWORDS | METH_STATIC, NULL },"
    )


def generate_factory_function(otype: ObjectType, ctor: Procedure, marshals: List["ParamMarshal"]) -> str:
    indent = " " * (len(otype.c_symbol_prefix) + len(ctor.name) + 3)

    decls = [line for m in marshals for line in m.decls]
    decls.append(f"{otype.c_type} * handle;")
    if ctor.throws:
        decls.append("GError * error = NULL;")
    decls.append("PyObject * result = NULL;")
    decls_block = indent_c_code("\n".join(decls), 1, prologue="\n")

    keyword_decl = ""
    parse_block = ""
    if marshals:
        keywords = ", ".join([f'"{m.keyword}"' for m in marshals] + ["NULL"])
        keyword_decl = f"\n  static char * keywords[] = {{ {keywords} }};"
        parse_block = "\n" + generate_argument_parse(marshals, on_failure="return NULL;")

    post_block = ""
    for m in marshals:
        if m.post is not None:
            post_block += indent_c_code(m.post, 1, prologue="\n") + "\n"

    call_args = [m.call_arg for m in marshals]
    if ctor.throws:
        call_args.append("&error")
    call = f"handle = ({otype.c_type} *) {ctor.c_identifier} ({', '.join(call_args)});"

    error_block = ""
    if ctor.throws:
        error_block = """
  if (error != NULL)
  {
    result = PyFrida_raise (error);
    goto beach;
  }
"""

    take_handle = f"result = PyGObject_new_take_handle (g_steal_pointer (&handle), PYFRIDA_TYPE ({otype.py_name}));"

    return f"""
static PyObject *
{otype.c_symbol_prefix}_{ctor.name} (PyObject * self,
{indent}PyObject * args,
{indent}PyObject * kw)
{{{keyword_decl}{decls_block}
{parse_block}{post_block}
  {call}
{error_block}
  {take_handle}

{generate_factory_tail(marshals, ctor.throws)}"""


def generate_factory_tail(marshals: List["ParamMarshal"], throws: bool) -> str:
    cleanups = [m.cleanup for m in marshals if m.cleanup is not None]
    cleanup_block = indent_c_code("\n".join(reversed(cleanups)), 1) if cleanups else ""

    if throws or constructor_needs_beach(marshals):
        prologue = f"{cleanup_block}\n\n" if cleanup_block else ""
        return f"""beach:
{prologue}  return result;
}}
"""
    if cleanup_block:
        return f"""{cleanup_block}

  return result;
}}
"""
    return """  return result;
}
"""


def generate_argument_parse(marshals: List["ParamMarshal"], on_failure: str = "return -1;") -> str:
    fmt = ""
    optional_started = False
    for m in marshals:
        if m.optional and not optional_started:
            fmt += "|"
            optional_started = True
        fmt += m.fmt
    parse_args = ["args", "kw", f'"{fmt}"', "keywords"]
    for m in marshals:
        parse_args += m.parse_args
    indent = " " * len("  if (!PyArg_ParseTupleAndKeywords (")
    parse_args_str = (",\n" + indent).join(parse_args)
    return f"""  if (!PyArg_ParseTupleAndKeywords ({parse_args_str}))
    {on_failure}
"""


def constructor_needs_beach(marshals: List["ParamMarshal"]) -> bool:
    return any(m.post is not None and "goto beach" in m.post for m in marshals)


def generate_constructor_tail(marshals: List["ParamMarshal"]) -> str:
    cleanups = [m.cleanup for m in marshals if m.cleanup is not None]
    if not cleanups:
        return """  return 0;
}
"""

    cleanup_block = indent_c_code("\n".join(reversed(cleanups)), 1)
    if constructor_needs_beach(marshals):
        return f"""  result = 0;

beach:
{cleanup_block}

  return result;
}}
"""
    return f"""{cleanup_block}

  return 0;
}}
"""


def constructor_indent(otype: ObjectType) -> str:
    return " " * (len(otype.c_symbol_prefix) + len("_init") + 2)


def build_param_marshal(param: Parameter) -> Optional["ParamMarshal"]:
    name = param.name
    type_name = param.type.name

    if type_name == "utf8":
        return ParamMarshal(
            keyword=name,
            fmt="es",
            decls=[f"char * {name} = NULL;"],
            parse_args=['"utf-8"', f"&{name}"],
            call_arg=name,
            cleanup=f"PyMem_Free ({name});",
        )

    numeric = PYARG_NUMERIC_FORMATS.get(type_name)
    if numeric is not None:
        fmt, c_type = numeric
        return ParamMarshal(
            keyword=name,
            fmt=fmt,
            decls=[f"{c_type} {name} = 0;"],
            parse_args=[f"&{name}"],
            call_arg=name,
        )

    enum = resolve_enumeration(param.type, param.object_type.model)
    if enum is not None:
        raw = f"{name}_value"
        return ParamMarshal(
            keyword=name,
            fmt="es",
            decls=[f"char * {raw} = NULL;", f"{param.type.c} {name} = 0;"],
            parse_args=['"utf-8"', f"&{raw}"],
            call_arg=name,
            post=f"""if ({raw} != NULL && !PyGObject_unmarshal_enum ({raw}, {enum.get_type} (), &{name}))
  goto beach;""",
            cleanup=f"PyMem_Free ({raw});",
        )

    if type_name == "GLib.Bytes":
        data = f"{name}_data"
        size = f"{name}_size"
        return ParamMarshal(
            keyword=name,
            fmt="z#",
            decls=[
                f"const char * {data} = NULL;",
                f"Py_ssize_t {size} = 0;",
                f"GBytes * {name} = NULL;",
            ],
            parse_args=[f"&{data}", f"&{size}"],
            call_arg=name,
            post=f"if ({data} != NULL)\n    {name} = g_bytes_new ({data}, {size});",
            cleanup=f"g_clear_pointer (&{name}, g_bytes_unref);",
        )

    if resolve_input_object_type(param.type, param.object_type.model) is not None:
        obj = f"{name}_obj"
        return ParamMarshal(
            keyword=name,
            fmt="O",
            decls=[f"PyObject * {obj} = NULL;", f"{param.type.c} {name} = NULL;"],
            parse_args=[f"&{obj}"],
            call_arg=name,
            post=f"""if ({obj} != NULL && {obj} != Py_None)
  {name} = ({param.type.c}) PY_GOBJECT_HANDLE ({obj});""",
        )

    return None


def resolve_enumeration(type: Type, model: Model) -> Optional[Enumeration]:
    tokens = type.name.split(".", maxsplit=1)
    if len(tokens) != 2:
        return None
    return model.enumerations.get(tokens[1])


class ParamMarshal:
    def __init__(
        self,
        keyword: str,
        fmt: str,
        decls: List[str],
        parse_args: List[str],
        call_arg: str,
        post: Optional[str] = None,
        cleanup: Optional[str] = None,
    ):
        self.keyword = keyword
        self.fmt = fmt
        self.decls = decls
        self.parse_args = parse_args
        self.call_arg = call_arg
        self.post = post
        self.cleanup = cleanup
        self.optional = False


PYARG_NUMERIC_FORMATS = {
    "gint8": ("b", "int"),
    "guint8": ("B", "unsigned int"),
    "gint16": ("h", "int"),
    "guint16": ("H", "unsigned int"),
    "gint": ("i", "int"),
    "gint32": ("i", "int"),
    "guint": ("I", "unsigned int"),
    "guint32": ("I", "unsigned int"),
    "gint64": ("L", "long long"),
    "guint64": ("K", "unsigned long long"),
    "gssize": ("n", "Py_ssize_t"),
    "gsize": ("n", "Py_ssize_t"),
}


def generate_destruction_code(variable: str, destroy_func: str):
    if destroy_func == "g_free":
        return f"g_free ({variable});"
    return f"g_clear_pointer (&{variable}, {destroy_func});"


def generate_object_type_typedefs(model: Model) -> str:
    return "\n".join(
        [
            f"typedef struct _{t.c_symbol_prefix} {t.c_symbol_prefix};"
            for t in model.regular_object_types
            if t.name != "Object"
        ]
    )


def generate_object_type_structs(model: Model) -> str:
    structs = []

    for otype in model.regular_object_types:
        if otype.name == "Object":
            continue
        structs.append(f"""struct _{otype.c_symbol_prefix}
{{
  {otype.parent_c_symbol_prefix} parent;
}};""")

    return "\n\n".join(structs)


def indent_c_code(code: str, level: int, prologue: str = "") -> str:
    prefix = (level * 2) * " "
    return indent_code(code, prefix, prologue)


def indent_code(code: str, prefix: str, prologue: str = "") -> str:
    if not code:
        return ""
    return prologue + textwrap.indent(code, prefix, lambda line: line.strip() != "")
