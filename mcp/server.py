"""
MCP Server for Frida

This module implements the MCP (Model Context Protocol) Server that exposes
Frida's dynamic instrumentation capabilities to AI Agents.

Usage:
    # Start as stdio server (default):
    python -m frida.mcp.server --stdio

    # Start as SSE server:
    python -m frida.mcp.server --sse --port 8080

    # Start as HTTP server:
    python -m frida.mcp.server --http --port 8080
"""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any, Dict

try:
    from mcp.server import Server
    from mcp.server.stdio import stdio_server
    from mcp.server.sse import SseServerTransport
    from mcp.types import Tool, TextContent
except ImportError:
    print("Error: MCP SDK not installed. Run: pip install mcp")
    sys.exit(1)

# Import Frida MCP tools
try:
    from . import tools as frida_tools
except ImportError:
    # Fallback for standalone execution
    import tools as frida_tools


def create_mcp_server() -> Server:
    """
    Create and configure the MCP Server instance.

    Returns:
        Configured MCP Server with all Frida tools registered
    """
    server = Server("frida-mcp-server")

    @server.list_tools()
    async def list_tools() -> list[Tool]:
        """List all available Frida tools."""
        tools = []
        for name, tool_info in frida_tools.TOOLS.items():
            tools.append(
                Tool(
                    name=name,
                    description=tool_info["description"],
                    inputSchema=tool_info["parameters"],
                )
            )
        return tools

    @server.call_tool()
    async def call_tool(name: str, arguments: Dict[str, Any]) -> list[TextContent]:
        """Call a Frida tool with the given arguments."""
        if name not in frida_tools.TOOLS:
            return [TextContent(type="text", text=json.dumps({"error": f"Unknown tool: {name}"}))]

        tool_func = frida_tools.TOOLS[name]["function"]

        try:
            # Call the tool function with arguments
            result = tool_func(**arguments)

            # Return result as JSON text
            return [TextContent(type="text", text=json.dumps(result, indent=2))]
        except Exception as e:
            return [TextContent(type="text", text=json.dumps({"error": str(e)}))]

    return server


async def run_stdio_server():
    """Run the MCP Server using stdio transport."""
    server = create_mcp_server()

    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream)


async def run_sse_server(host: str = "0.0.0.0", port: int = 8080):
    """Run the MCP Server using SSE transport."""
    try:
        from starlette.applications import Starlette
        from starlette.routing import Route
        import uvicorn
    except ImportError:
        print("Error: SSE server requires additional dependencies. Run: pip install starlette uvicorn")
        sys.exit(1)

    server = create_mcp_server()
    sse = SseServerTransport("/messages")

    async def handle_sse(request):
        async with sse.connect_sse(request.scope, request.receive, request._send) as streams:
            await server.run(streams[0], streams[1])

    async def handle_messages(request):
        await sse.handle_request_message(request.scope, request.receive, request._send)

    app = Starlette(
        routes=[
            Route("/sse", endpoint=handle_sse),
            Route("/messages", endpoint=handle_messages, methods=["POST"]),
        ],
    )

    config = uvicorn.Config(app, host=host, port=port, log_level="info")
    uv_server = uvicorn.Server(config)
    await uv_server.serve()


async def run_http_server(host: str = "0.0.0.0", port: int = 8080):
    """Run the MCP Server using HTTP transport."""
    try:
        from mcp.server.streamable_http import StreamableHTTPServer
    except ImportError:
        print("Error: HTTP transport requires newer MCP SDK version. Falling back to SSE.")
        await run_sse_server(host, port)
        return

    server = create_mcp_server()
    http_server = StreamableHTTPServer(server, host=host, port=port)
    await http_server.serve()


def main():
    """Main entry point for the MCP Server."""
    parser = argparse.ArgumentParser(description="Frida MCP Server")
    parser.add_argument(
        "--stdio",
        action="store_true",
        help="Use stdio transport (default)",
    )
    parser.add_argument(
        "--sse",
        action="store_true",
        help="Use SSE transport",
    )
    parser.add_argument(
        "--http",
        action="store_true",
        help="Use HTTP transport",
    )
    parser.add_argument(
        "--host",
        type=str,
        default="0.0.0.0",
        help="Host to bind to (default: 0.0.0.0)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Port to bind to (default: 8080)",
    )
    parser.add_argument(
        "--list-tools",
        action="store_true",
        help="List available tools and exit",
    )

    args = parser.parse_args()

    if args.list_tools:
        print("Available Frida MCP tools:")
        for name, tool_info in frida_tools.TOOLS.items():
            print(f"  - {name}: {tool_info['description']}")
        return

    import asyncio

    # Determine transport mode
    if args.sse:
        print(f"Starting Frida MCP Server with SSE transport on {args.host}:{args.port}")
        asyncio.run(run_sse_server(args.host, args.port))
    elif args.http:
        print(f"Starting Frida MCP Server with HTTP transport on {args.host}:{args.port}")
        asyncio.run(run_http_server(args.host, args.port))
    else:
        # Default to stdio
        asyncio.run(run_stdio_server())


if __name__ == "__main__":
    main()
