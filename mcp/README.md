# Frida MCP Server

MCP (Model Context Protocol) Server implementation for Frida, enabling AI Agents to interact with Frida's dynamic instrumentation toolkit programmatically.

## Features

- **Process Management**: Attach/detach processes, list running processes
- **Function Hooking**: Install interceptor, stalker, and breakpoint hooks
- **Memory Operations**: Read/write process memory with hex/ASCII representation
- **Module Analysis**: Find loaded modules, resolve symbol addresses
- **Script Execution**: Execute custom Frida JavaScript scripts
- **Session Management**: Multiple concurrent sessions with unique IDs

## Installation

### Prerequisites

- Python 3.8+
- Frida (`pip install frida`)
- MCP SDK (`pip install mcp`)

### Install Dependencies

```bash
pip install frida mcp
```

For SSE/HTTP transport support:

```bash
pip install frida mcp starlette uvicorn
```

## Usage

### Starting the Server

#### stdio Transport (Default)

```bash
python -m frida.mcp.server --stdio
```

#### SSE Transport

```bash
python -m frida.mcp.server --sse --host 0.0.0.0 --port 8080
```

#### HTTP Transport

```bash
python -m frida.mcp.server --http --host 0.0.0.0 --port 8080
```

### List Available Tools

```bash
python -m frida.mcp.server --list-tools
```

## Available Tools

### 1. attach_process

Attach to a process by PID or name.

**Parameters:**
- `pid` (integer, optional): Process ID to attach to
- `name` (string, optional): Process name to attach to
- `device_id` (string, default: "local"): Device ID

**Example:**
```json
{
  "pid": 1234,
  "device_id": "local"
}
```

### 2. detach_process

Detach from a process.

**Parameters:**
- `session_id` (string, required): Session ID to detach

**Example:**
```json
{
  "session_id": "abc12345"
}
```

### 3. list_processes

List available processes.

**Parameters:**
- `device_id` (string, default: "local"): Device ID

**Example:**
```json
{
  "device_id": "local"
}
```

### 4. install_hook

Install a function hook.

**Parameters:**
- `session_id` (string, required): Session ID
- `module` (string, required): Module name containing the function
- `symbol` (string, required): Symbol/function name to hook
- `callback_script` (string, required): JavaScript callback code
- `hook_type` (string, default: "interceptor"): Type of hook ("interceptor", "stalker", "breakpoint")

**Example:**
```json
{
  "session_id": "abc12345",
  "module": "libc.so",
  "symbol": "printf",
  "callback_script": "console.log('printf called with:', args[0].readUtf8String());",
  "hook_type": "interceptor"
}
```

### 5. remove_hook

Remove a hook.

**Parameters:**
- `hook_id` (string, required): Hook ID to remove

**Example:**
```json
{
  "hook_id": "hook123"
}
```

### 6. read_memory

Read process memory.

**Parameters:**
- `session_id` (string, required): Session ID
- `address` (integer, required): Memory address to read from
- `size` (integer, default: 64): Number of bytes to read

**Example:**
```json
{
  "session_id": "abc12345",
  "address": 0x400000,
  "size": 256
}
```

### 7. write_memory

Write to process memory.

**Parameters:**
- `session_id` (string, required): Session ID
- `address` (integer, required): Memory address to write to
- `data` (string, required): Hex string data to write

**Example:**
```json
{
  "session_id": "abc12345",
  "address": 0x400000,
  "data": "90909090"
}
```

### 8. find_modules

Find loaded modules.

**Parameters:**
- `session_id` (string, required): Session ID
- `pattern` (string, optional): Pattern to filter modules (glob-style)

**Example:**
```json
{
  "session_id": "abc12345",
  "pattern": "lib*.so"
}
```

### 9. resolve_symbol

Resolve symbol address.

**Parameters:**
- `session_id` (string, required): Session ID
- `module` (string, required): Module name
- `symbol` (string, required): Symbol name

**Example:**
```json
{
  "session_id": "abc12345",
  "module": "libc.so",
  "symbol": "printf"
}
```

### 10. execute_script

Execute Frida JavaScript script.

**Parameters:**
- `session_id` (string, required): Session ID
- `script_code` (string, required): JavaScript code to execute
- `timeout` (number, default: 5.0): Timeout in seconds

**Example:**
```json
{
  "session_id": "abc12345",
  "script_code": "console.log('Hello from Frida!');",
  "timeout": 10.0
}
```

## AI Agent Integration

### Using with Claude Desktop

Add to your Claude Desktop configuration (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "frida": {
      "command": "python",
      "args": ["-m", "frida.mcp.server", "--stdio"]
    }
  }
}
```

### Using with Other MCP Clients

Connect to the server using any MCP client:

```python
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def main():
    server_params = StdioServerParameters(
        command="python",
        args=["-m", "frida.mcp.server", "--stdio"]
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            # List tools
            tools = await session.list_tools()
            print(tools)

            # Call a tool
            result = await session.call_tool(
                "list_processes",
                {"device_id": "local"}
            )
            print(result)
```

## Example Workflow

### 1. Attach to a Process

```python
result = attach_process(pid=1234)
session_id = result["id"]
```

### 2. Install a Hook

```python
result = install_hook(
    session_id=session_id,
    module="libc.so",
    symbol="printf",
    callback_script="console.log('printf called');",
    hook_type="interceptor"
)
hook_id = result["id"]
```

### 3. Read Memory

```python
result = read_memory(
    session_id=session_id,
    address=0x400000,
    size=256
)
print(result["hex"])
```

### 4. Execute Custom Script

```python
result = execute_script(
    session_id=session_id,
    script_code="""
    var modules = Process.enumerateModules();
    send({type: 'output', data: JSON.stringify(modules, null, 2)});
    """
)
print(result["output"])
```

### 5. Cleanup

```python
remove_hook(hook_id)
detach_process(session_id)
```

## Data Models

### Process
```python
{
    "pid": int,
    "name": str,
    "path": str,
    "parameters": List[str]
}
```

### Session
```python
{
    "id": str,
    "target_pid": int,
    "target_name": str,
    "status": str,  # "attached", "detached", "lost"
    "device_id": str,
    "created_at": str
}
```

### Hook
```python
{
    "id": str,
    "address": str,  # hex string
    "type": str,  # "interceptor", "stalker", "breakpoint"
    "callback": str,
    "module": str,
    "symbol": str,
    "enabled": bool
}
```

### MemoryRegion
```python
{
    "base": str,  # hex string
    "size": int,
    "end": str,  # hex string
    "protection": str,  # "r--", "rw-", "r-x", "rwx"
    "type": str,
    "file": str,
    "file_offset": str  # hex string
}
```

## Testing

Run the test suite:

```bash
pytest tests/test_mcp_server.py -v
```

## Architecture

```
frida/mcp/
├── __init__.py       # Package initialization
├── server.py         # MCP Server implementation
├── tools.py          # Tool implementations
└── models.py         # Data models
```

## Security Considerations

- **Process Access**: Attaching to processes requires appropriate permissions
- **Memory Operations**: Reading/writing memory can crash target processes
- **Code Execution**: Hooks execute arbitrary JavaScript in target process context
- **Remote Devices**: Be cautious when attaching to remote devices

## Troubleshooting

### Frida Not Found

```bash
pip install frida
```

### Permission Denied

Run with appropriate permissions or use sudo/administrator privileges.

### Connection Refused

Ensure the target process is running and Frida server is accessible.

## License

This implementation follows the same license as the parent Frida project.

## Contributing

Contributions are welcome! Please ensure:
- All tests pass
- Code follows existing style
- New features include tests
- Documentation is updated

## Resources

- [Frida Documentation](https://frida.re/docs/home/)
- [MCP Specification](https://modelcontextprotocol.io/)
- [Frida Python Bindings](https://github.com/frida/frida-python)
