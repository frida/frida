"""
MCP Tools implementation for Frida

This module implements the MCP tools that expose Frida's core functionality
to AI Agents via the Model Context Protocol.
"""

from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any, Dict, List, Optional

from .models import (
    CommandResult,
    Hook,
    MemoryContent,
    MemoryRegion,
    Module,
    Process,
    ScriptResult,
    Session,
    Symbol,
)

# Global session registry
_sessions: Dict[str, Dict[str, Any]] = {}
_hooks: Dict[str, Hook] = {}


def _get_frida():
    """Lazy import frida to allow testing without frida installed."""
    try:
        import frida
        return frida
    except ImportError:
        raise RuntimeError(
            "Frida not installed. Run: pip install frida"
        )


def attach_process(
    pid: Optional[int] = None,
    name: Optional[str] = None,
    device_id: str = "local"
) -> Dict[str, Any]:
    """
    Attach to a process by PID or name.

    Args:
        pid: Process ID to attach to
        name: Process name to attach to (alternative to pid)
        device_id: Device ID (default: "local")

    Returns:
        Session object with attachment details
    """
    try:
        frida = _get_frida()

        # Get device
        if device_id == "local":
            device = frida.get_local_device()
        else:
            device = frida.get_device(device_id)

        # Attach by PID or name
        if pid is not None:
            session = device.attach(pid)
            target_pid = pid
            # Try to get process name
            try:
                procs = device.enumerate_processes()
                target_name = next((p.name for p in procs if p.pid == pid), "")
            except Exception:
                target_name = ""
        elif name is not None:
            session = device.attach(name)
            # Get PID from process list
            procs = device.enumerate_processes()
            proc = next((p for p in procs if p.name == name), None)
            target_pid = proc.pid if proc else 0
            target_name = name
        else:
            return {"error": "Either pid or name must be provided"}

        # Create session ID
        session_id = str(uuid.uuid4())[:8]

        # Store session
        _sessions[session_id] = {
            "frida_session": session,
            "device": device,
            "target_pid": target_pid,
            "target_name": target_name,
            "created_at": datetime.now().isoformat(),
        }

        result = Session(
            id=session_id,
            target_pid=target_pid,
            target_name=target_name,
            status="attached",
            device_id=device_id,
            created_at=_sessions[session_id]["created_at"],
        )
        return result.to_dict()

    except Exception as e:
        return {"error": str(e)}


def detach_process(session_id: str) -> Dict[str, Any]:
    """
    Detach from a process.

    Args:
        session_id: Session ID to detach

    Returns:
        Result with success status
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Detach
        frida_session.detach()

        # Remove from registry
        del _sessions[session_id]

        # Remove associated hooks
        hooks_to_remove = [h_id for h_id, h in _hooks.items()
                          if h.callback == session_id]
        for h_id in hooks_to_remove:
            del _hooks[h_id]

        return {
            "session_id": session_id,
            "status": "detached",
            "message": "Successfully detached from process"
        }

    except Exception as e:
        return {"error": str(e)}


def list_processes(device_id: str = "local") -> Dict[str, Any]:
    """
    List available processes.

    Args:
        device_id: Device ID (default: "local")

    Returns:
        List of Process objects
    """
    try:
        frida = _get_frida()

        # Get device
        if device_id == "local":
            device = frida.get_local_device()
        else:
            device = frida.get_device(device_id)

        # Enumerate processes
        procs = device.enumerate_processes()

        processes = []
        for proc in procs:
            p = Process(
                pid=proc.pid,
                name=proc.name,
                parameters=proc.parameters if hasattr(proc, 'parameters') else []
            )
            processes.append(p.to_dict())

        return {
            "device_id": device_id,
            "count": len(processes),
            "processes": processes
        }

    except Exception as e:
        return {"error": str(e)}


def install_hook(
    session_id: str,
    module: str,
    symbol: str,
    callback_script: str,
    hook_type: str = "interceptor"
) -> Dict[str, Any]:
    """
    Install a function hook.

    Args:
        session_id: Session ID
        module: Module name containing the function
        symbol: Symbol/function name to hook
        callback_script: JavaScript callback code
        hook_type: Type of hook ("interceptor", "stalker", "breakpoint")

    Returns:
        Hook object with details
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Create hook script
        if hook_type == "interceptor":
            script_code = f"""
            var ptr = Module.findExportByName("{module}", "{symbol}");
            if (ptr !== null) {{
                Interceptor.attach(ptr, {{
                    onEnter: function(args) {{
                        {callback_script}
                    }}
                }});
                send({{type: 'hook_installed', address: ptr.toString()}});
            }} else {{
                send({{type: 'error', message: 'Symbol not found'}});
            }}
            """
        elif hook_type == "stalker":
            script_code = f"""
            var ptr = Module.findExportByName("{module}", "{symbol}");
            if (ptr !== null) {{
                Stalker.follow({{
                    events: {{
                        call: true,
                        ret: true
                    }},
                    onReceive: function(events) {{
                        {callback_script}
                    }}
                }});
                send({{type: 'hook_installed', address: ptr.toString()}});
            }} else {{
                send({{type: 'error', message: 'Symbol not found'}});
            }}
            """
        elif hook_type == "breakpoint":
            script_code = f"""
            var ptr = Module.findExportByName("{module}", "{symbol}");
            if (ptr !== null) {{
                Interceptor.attach(ptr, {{
                    onEnter: function(args) {{
                        this.context.pc = this.context.pc;
                        {callback_script}
                    }}
                }});
                send({{type: 'hook_installed', address: ptr.toString()}});
            }} else {{
                send({{type: 'error', message: 'Symbol not found'}});
            }}
            """
        else:
            return {"error": f"Unknown hook type: {hook_type}"}

        # Create and load script
        script = frida_session.create_script(script_code)

        # Handle messages
        hook_id = str(uuid.uuid4())[:8]
        address = 0

        def on_message(message, data):
            nonlocal address
            if message['type'] == 'send':
                payload = message['payload']
                if payload.get('type') == 'hook_installed':
                    address = int(payload.get('address', '0x0'), 16)
                elif payload.get('type') == 'error':
                    raise Exception(payload.get('message', 'Unknown error'))

        script.on('message', on_message)
        script.load()

        # Store hook
        hook = Hook(
            id=hook_id,
            address=address,
            type=hook_type,
            callback=callback_script,
            module=module,
            symbol=symbol,
            enabled=True
        )
        _hooks[hook_id] = hook

        return hook.to_dict()

    except Exception as e:
        return {"error": str(e)}


def remove_hook(hook_id: str) -> Dict[str, Any]:
    """
    Remove a hook.

    Args:
        hook_id: Hook ID to remove

    Returns:
        Result with success status
    """
    try:
        if hook_id not in _hooks:
            return {"error": f"Hook not found: {hook_id}"}

        # Remove from registry
        del _hooks[hook_id]

        return {
            "hook_id": hook_id,
            "status": "removed",
            "message": "Hook removed successfully"
        }

    except Exception as e:
        return {"error": str(e)}


def read_memory(
    session_id: str,
    address: int,
    size: int = 64
) -> Dict[str, Any]:
    """
    Read process memory.

    Args:
        session_id: Session ID
        address: Memory address to read from
        size: Number of bytes to read (default: 64)

    Returns:
        MemoryContent with hex and ASCII representation
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Read memory using script
        script_code = f"""
        var address = ptr("{hex(address)}");
        var data = address.readByteArray({size});
        send({{type: 'memory', data: Array.from(new Uint8Array(data))}});
        """

        script = frida_session.create_script(script_code)
        memory_data = None

        def on_message(message, data):
            nonlocal memory_data
            if message['type'] == 'send':
                payload = message['payload']
                if payload.get('type') == 'memory':
                    memory_data = bytes(payload.get('data', []))

        script.on('message', on_message)
        script.load()

        # Wait for data (simplified - real impl would use threading.Event)
        import time
        time.sleep(0.1)
        script.unload()

        if memory_data is None:
            return {"error": "Failed to read memory"}

        # Create ASCII representation
        ascii_repr = ""
        for byte in memory_data:
            if 32 <= byte <= 126:
                ascii_repr += chr(byte)
            else:
                ascii_repr += "."

        content = MemoryContent(
            address=address,
            data=memory_data,
            ascii_repr=ascii_repr
        )
        return content.to_dict()

    except Exception as e:
        return {"error": str(e), "address": hex(address)}


def write_memory(
    session_id: str,
    address: int,
    data: str
) -> Dict[str, Any]:
    """
    Write to process memory.

    Args:
        session_id: Session ID
        address: Memory address to write to
        data: Hex string data to write

    Returns:
        Result with success status
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Convert hex string to bytes
        data_bytes = bytes.fromhex(data.replace("0x", "").replace(" ", ""))

        # Write memory using script
        data_array = list(data_bytes)
        script_code = f"""
        var address = ptr("{hex(address)}");
        var data = new Uint8Array({data_array});
        address.writeByteArray(data);
        send({{type: 'written', size: {len(data_bytes)}}});
        """

        script = frida_session.create_script(script_code)
        written = False

        def on_message(message, data):
            nonlocal written
            if message['type'] == 'send':
                payload = message['payload']
                if payload.get('type') == 'written':
                    written = True

        script.on('message', on_message)
        script.load()

        import time
        time.sleep(0.1)
        script.unload()

        return {
            "address": hex(address),
            "size": len(data_bytes),
            "status": "written" if written else "failed"
        }

    except Exception as e:
        return {"error": str(e), "address": hex(address)}


def find_modules(session_id: str, pattern: str = "") -> Dict[str, Any]:
    """
    Find loaded modules.

    Args:
        session_id: Session ID
        pattern: Optional pattern to filter modules (glob-style)

    Returns:
        List of Module objects
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Enumerate modules using script
        script_code = """
        var modules = Process.enumerateModules();
        send({type: 'modules', modules: modules.map(m => ({
            name: m.name,
            base: m.base.toString(),
            size: m.size,
            path: m.path
        }))});
        """

        script = frida_session.create_script(script_code)
        modules_data = []

        def on_message(message, data):
            nonlocal modules_data
            if message['type'] == 'send':
                payload = message['payload']
                if payload.get('type') == 'modules':
                    modules_data = payload.get('modules', [])

        script.on('message', on_message)
        script.load()

        import time
        time.sleep(0.1)
        script.unload()

        # Filter by pattern if provided
        if pattern:
            import fnmatch
            modules_data = [m for m in modules_data
                          if fnmatch.fnmatch(m['name'], pattern)]

        # Convert to Module objects
        modules = []
        for m in modules_data:
            module = Module(
                name=m['name'],
                base=int(m['base'], 16),
                size=m['size'],
                path=m['path']
            )
            modules.append(module.to_dict())

        return {
            "count": len(modules),
            "modules": modules
        }

    except Exception as e:
        return {"error": str(e)}


def resolve_symbol(
    session_id: str,
    module: str,
    symbol: str
) -> Dict[str, Any]:
    """
    Resolve symbol address.

    Args:
        session_id: Session ID
        module: Module name
        symbol: Symbol name

    Returns:
        Symbol object with address
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Resolve symbol using script
        script_code = f"""
        var ptr = Module.findExportByName("{module}", "{symbol}");
        if (ptr !== null) {{
            send({{type: 'symbol', address: ptr.toString(), name: "{symbol}"}});
        }} else {{
            send({{type: 'not_found', name: "{symbol}"}});
        }}
        """

        script = frida_session.create_script(script_code)
        symbol_info = None

        def on_message(message, data):
            nonlocal symbol_info
            if message['type'] == 'send':
                payload = message['payload']
                if payload.get('type') == 'symbol':
                    symbol_info = {
                        'address': int(payload.get('address', '0x0'), 16),
                        'name': payload.get('name', symbol)
                    }
                elif payload.get('type') == 'not_found':
                    symbol_info = {'not_found': True}

        script.on('message', on_message)
        script.load()

        import time
        time.sleep(0.1)
        script.unload()

        if symbol_info is None:
            return {"error": "Failed to resolve symbol"}

        if symbol_info.get('not_found'):
            return {"error": f"Symbol not found: {symbol}"}

        sym = Symbol(
            name=symbol_info['name'],
            address=symbol_info['address'],
            type="function",
            module=module
        )
        return sym.to_dict()

    except Exception as e:
        return {"error": str(e)}


def execute_script(
    session_id: str,
    script_code: str,
    timeout: float = 5.0
) -> Dict[str, Any]:
    """
    Execute Frida JavaScript script.

    Args:
        session_id: Session ID
        script_code: JavaScript code to execute
        timeout: Timeout in seconds (default: 5.0)

    Returns:
        ScriptResult with output and logs
    """
    try:
        if session_id not in _sessions:
            return {"error": f"Session not found: {session_id}"}

        session_data = _sessions[session_id]
        frida_session = session_data["frida_session"]

        # Create script
        script = frida_session.create_script(script_code)

        output = ""
        logs = []
        error = ""
        return_code = 0

        def on_message(message, data):
            nonlocal output, error, return_code
            if message['type'] == 'send':
                payload = message.get('payload', '')
                if isinstance(payload, dict):
                    if payload.get('type') == 'log':
                        logs.append(payload.get('message', ''))
                    elif payload.get('type') == 'output':
                        output += payload.get('data', '')
                    elif payload.get('type') == 'error':
                        error = payload.get('message', 'Unknown error')
                        return_code = 1
                else:
                    output += str(payload)
            elif message['type'] == 'error':
                error = message.get('description', 'Script error')
                return_code = 1

        script.on('message', on_message)

        try:
            script.load()
            # Wait for completion or timeout
            import time
            time.sleep(min(timeout, 1.0))
            script.unload()
        except Exception as e:
            error = str(e)
            return_code = 1

        result = ScriptResult(
            output=output,
            error=error,
            return_code=return_code,
            logs=logs
        )
        return result.to_dict()

    except Exception as e:
        return {"error": str(e)}


# Tool registry for MCP Server
TOOLS = {
    "attach_process": {
        "function": attach_process,
        "description": "Attach to a process by PID or name",
        "parameters": {
            "type": "object",
            "properties": {
                "pid": {
                    "type": "integer",
                    "description": "Process ID to attach to",
                },
                "name": {
                    "type": "string",
                    "description": "Process name to attach to (alternative to pid)",
                },
                "device_id": {
                    "type": "string",
                    "description": "Device ID (default: 'local')",
                    "default": "local",
                },
            },
        },
    },
    "detach_process": {
        "function": detach_process,
        "description": "Detach from a process",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID to detach",
                },
            },
            "required": ["session_id"],
        },
    },
    "list_processes": {
        "function": list_processes,
        "description": "List available processes",
        "parameters": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Device ID (default: 'local')",
                    "default": "local",
                },
            },
        },
    },
    "install_hook": {
        "function": install_hook,
        "description": "Install a function hook",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "module": {
                    "type": "string",
                    "description": "Module name containing the function",
                },
                "symbol": {
                    "type": "string",
                    "description": "Symbol/function name to hook",
                },
                "callback_script": {
                    "type": "string",
                    "description": "JavaScript callback code",
                },
                "hook_type": {
                    "type": "string",
                    "description": "Type of hook ('interceptor', 'stalker', 'breakpoint')",
                    "default": "interceptor",
                },
            },
            "required": ["session_id", "module", "symbol", "callback_script"],
        },
    },
    "remove_hook": {
        "function": remove_hook,
        "description": "Remove a hook",
        "parameters": {
            "type": "object",
            "properties": {
                "hook_id": {
                    "type": "string",
                    "description": "Hook ID to remove",
                },
            },
            "required": ["hook_id"],
        },
    },
    "read_memory": {
        "function": read_memory,
        "description": "Read process memory",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "address": {
                    "type": "integer",
                    "description": "Memory address to read from (in hex or decimal)",
                },
                "size": {
                    "type": "integer",
                    "description": "Number of bytes to read (default: 64)",
                    "default": 64,
                },
            },
            "required": ["session_id", "address"],
        },
    },
    "write_memory": {
        "function": write_memory,
        "description": "Write to process memory",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "address": {
                    "type": "integer",
                    "description": "Memory address to write to",
                },
                "data": {
                    "type": "string",
                    "description": "Hex string data to write",
                },
            },
            "required": ["session_id", "address", "data"],
        },
    },
    "find_modules": {
        "function": find_modules,
        "description": "Find loaded modules",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "pattern": {
                    "type": "string",
                    "description": "Optional pattern to filter modules (glob-style)",
                    "default": "",
                },
            },
            "required": ["session_id"],
        },
    },
    "resolve_symbol": {
        "function": resolve_symbol,
        "description": "Resolve symbol address",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "module": {
                    "type": "string",
                    "description": "Module name",
                },
                "symbol": {
                    "type": "string",
                    "description": "Symbol name",
                },
            },
            "required": ["session_id", "module", "symbol"],
        },
    },
    "execute_script": {
        "function": execute_script,
        "description": "Execute Frida JavaScript script",
        "parameters": {
            "type": "object",
            "properties": {
                "session_id": {
                    "type": "string",
                    "description": "Session ID",
                },
                "script_code": {
                    "type": "string",
                    "description": "JavaScript code to execute",
                },
                "timeout": {
                    "type": "number",
                    "description": "Timeout in seconds (default: 5.0)",
                    "default": 5.0,
                },
            },
            "required": ["session_id", "script_code"],
        },
    },
}
