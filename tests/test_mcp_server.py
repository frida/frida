"""
Tests for Frida MCP Server

This module contains unit tests for the Frida MCP Server tools and models.
Tests use mocking to avoid dependency on real Frida processes.
"""

import pytest
from unittest.mock import Mock, MagicMock, patch
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from mcp.models import (
    Process,
    Session,
    Hook,
    MemoryRegion,
    Module,
    Symbol,
    MemoryContent,
    ScriptResult,
    CommandResult,
)
from mcp.tools import (
    attach_process,
    detach_process,
    list_processes,
    install_hook,
    remove_hook,
    read_memory,
    write_memory,
    find_modules,
    resolve_symbol,
    execute_script,
    _sessions,
    _hooks,
)


class TestModels:
    """Test data models."""

    def test_process_model(self):
        """Test Process model serialization."""
        proc = Process(pid=1234, name="test_process", path="/usr/bin/test")
        result = proc.to_dict()

        assert result["pid"] == 1234
        assert result["name"] == "test_process"
        assert result["path"] == "/usr/bin/test"
        assert result["parameters"] == []

    def test_session_model(self):
        """Test Session model serialization."""
        session = Session(
            id="abc123",
            target_pid=1234,
            target_name="test",
            status="attached",
            device_id="local"
        )
        result = session.to_dict()

        assert result["id"] == "abc123"
        assert result["target_pid"] == 1234
        assert result["target_name"] == "test"
        assert result["status"] == "attached"
        assert result["device_id"] == "local"

    def test_hook_model(self):
        """Test Hook model serialization."""
        hook = Hook(
            id="hook123",
            address=0x400000,
            type="interceptor",
            module="libc.so",
            symbol="printf"
        )
        result = hook.to_dict()

        assert result["id"] == "hook123"
        assert result["address"] == "0x400000"
        assert result["type"] == "interceptor"
        assert result["module"] == "libc.so"
        assert result["symbol"] == "printf"

    def test_memory_region_model(self):
        """Test MemoryRegion model serialization."""
        region = MemoryRegion(
            base=0x1000,
            size=0x1000,
            protection="r-x",
            type="code"
        )
        result = region.to_dict()

        assert result["base"] == "0x1000"
        assert result["size"] == 0x1000
        assert result["end"] == "0x2000"
        assert result["protection"] == "r-x"

    def test_module_model(self):
        """Test Module model serialization."""
        module = Module(
            name="libc.so",
            base=0x7f000000,
            size=0x200000,
            path="/lib/libc.so"
        )
        result = module.to_dict()

        assert result["name"] == "libc.so"
        assert result["base"] == "0x7f000000"
        assert result["size"] == 0x200000

    def test_symbol_model(self):
        """Test Symbol model serialization."""
        symbol = Symbol(
            name="printf",
            address=0x400500,
            type="function",
            module="libc.so"
        )
        result = symbol.to_dict()

        assert result["name"] == "printf"
        assert result["address"] == "0x400500"
        assert result["type"] == "function"

    def test_memory_content_model(self):
        """Test MemoryContent model serialization."""
        content = MemoryContent(
            address=0x1000,
            data=b"Hello World",
            ascii_repr="Hello World"
        )
        result = content.to_dict()

        assert result["address"] == "0x1000"
        assert result["size"] == 11
        assert "48656c6c6f" in result["hex"]  # "Hello" in hex

    def test_script_result_model(self):
        """Test ScriptResult model serialization."""
        result = ScriptResult(
            output="test output",
            error="",
            return_code=0,
            logs=["log1", "log2"]
        )
        data = result.to_dict()

        assert data["output"] == "test output"
        assert data["return_code"] == 0
        assert len(data["logs"]) == 2


class TestTools:
    """Test MCP tools with mocking."""

    def setup_method(self):
        """Clear session and hook registries before each test."""
        _sessions.clear()
        _hooks.clear()

    @patch('mcp.tools._get_frida')
    def test_attach_process_by_pid(self, mock_get_frida):
        """Test attaching to process by PID."""
        # Setup mock
        mock_frida = Mock()
        mock_device = Mock()
        mock_session = Mock()

        mock_get_frida.return_value = mock_frida
        mock_frida.get_local_device.return_value = mock_device
        mock_device.attach.return_value = mock_session
        mock_device.enumerate_processes.return_value = [
            Mock(pid=1234, name="test_process", parameters=[])
        ]

        # Call tool
        result = attach_process(pid=1234)

        # Verify
        assert "error" not in result
        assert result["target_pid"] == 1234
        assert result["target_name"] == "test_process"
        assert result["status"] == "attached"
        assert "id" in result

    @patch('mcp.tools._get_frida')
    def test_attach_process_by_name(self, mock_get_frida):
        """Test attaching to process by name."""
        # Setup mock
        mock_frida = Mock()
        mock_device = Mock()
        mock_session = Mock()

        mock_get_frida.return_value = mock_frida
        mock_frida.get_local_device.return_value = mock_device
        mock_device.attach.return_value = mock_session
        mock_device.enumerate_processes.return_value = [
            Mock(pid=1234, name="test_process", parameters=[])
        ]

        # Call tool
        result = attach_process(name="test_process")

        # Verify
        assert "error" not in result
        assert result["target_pid"] == 1234
        assert result["target_name"] == "test_process"

    @patch('mcp.tools._get_frida')
    def test_attach_process_no_target(self, mock_get_frida):
        """Test attaching without PID or name."""
        result = attach_process()

        assert "error" in result
        assert "pid or name" in result["error"]

    def test_detach_process(self):
        """Test detaching from process."""
        # Setup mock session
        mock_session = Mock()
        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        # Call tool
        result = detach_process("test123")

        # Verify
        assert "error" not in result
        assert result["status"] == "detached"
        mock_session.detach.assert_called_once()
        assert "test123" not in _sessions

    def test_detach_process_not_found(self):
        """Test detaching from non-existent session."""
        result = detach_process("nonexistent")

        assert "error" in result
        assert "not found" in result["error"]

    @patch('mcp.tools._get_frida')
    def test_list_processes(self, mock_get_frida):
        """Test listing processes."""
        # Setup mock
        mock_frida = Mock()
        mock_device = Mock()

        mock_get_frida.return_value = mock_frida
        mock_frida.get_local_device.return_value = mock_device
        mock_device.enumerate_processes.return_value = [
            Mock(pid=1, name="init", parameters=[]),
            Mock(pid=1234, name="test", parameters=[]),
        ]

        # Call tool
        result = list_processes()

        # Verify
        assert "error" not in result
        assert result["count"] == 2
        assert len(result["processes"]) == 2
        assert result["processes"][0]["pid"] == 1
        assert result["processes"][1]["name"] == "test"

    @patch('mcp.tools._get_frida')
    def test_install_hook_interceptor(self, mock_get_frida):
        """Test installing an interceptor hook."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate message callback
        def mock_on(event, callback):
            if event == 'message':
                # Simulate hook installed message
                callback({
                    'type': 'send',
                    'payload': {'type': 'hook_installed', 'address': '0x400500'}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = install_hook(
            session_id="test123",
            module="libc.so",
            symbol="printf",
            callback_script="console.log('hooked');",
            hook_type="interceptor"
        )

        # Verify
        assert "error" not in result
        assert result["type"] == "interceptor"
        assert result["module"] == "libc.so"
        assert result["symbol"] == "printf"
        assert result["address"] == "0x400500"

    def test_remove_hook(self):
        """Test removing a hook."""
        # Setup mock hook
        _hooks["hook123"] = Hook(
            id="hook123",
            address=0x400000,
            type="interceptor"
        )

        # Call tool
        result = remove_hook("hook123")

        # Verify
        assert "error" not in result
        assert result["status"] == "removed"
        assert "hook123" not in _hooks

    def test_remove_hook_not_found(self):
        """Test removing non-existent hook."""
        result = remove_hook("nonexistent")

        assert "error" in result
        assert "not found" in result["error"]

    @patch('mcp.tools._get_frida')
    def test_read_memory(self, mock_get_frida):
        """Test reading memory."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate memory read
        test_data = [72, 101, 108, 108, 111]  # "Hello"

        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'memory', 'data': test_data}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = read_memory("test123", 0x1000, 5)

        # Verify
        assert "error" not in result
        assert result["address"] == "0x1000"
        assert result["size"] == 5
        assert "48656c6c6f" in result["hex"]  # "Hello" in hex
        assert result["ascii"] == "Hello"

    @patch('mcp.tools._get_frida')
    def test_write_memory(self, mock_get_frida):
        """Test writing memory."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate write confirmation
        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'written', 'size': 5}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = write_memory("test123", 0x1000, "48656c6c6f")

        # Verify
        assert "error" not in result
        assert result["address"] == "0x1000"
        assert result["size"] == 5
        assert result["status"] == "written"

    @patch('mcp.tools._get_frida')
    def test_find_modules(self, mock_get_frida):
        """Test finding modules."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate module enumeration
        test_modules = [
            {'name': 'libc.so', 'base': '0x7f000000', 'size': 0x200000, 'path': '/lib/libc.so'},
            {'name': 'libm.so', 'base': '0x7f200000', 'size': 0x100000, 'path': '/lib/libm.so'},
        ]

        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'modules', 'modules': test_modules}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = find_modules("test123")

        # Verify
        assert "error" not in result
        assert result["count"] == 2
        assert result["modules"][0]["name"] == "libc.so"
        assert result["modules"][1]["name"] == "libm.so"

    @patch('mcp.tools._get_frida')
    def test_find_modules_with_pattern(self, mock_get_frida):
        """Test finding modules with pattern filter."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate module enumeration
        test_modules = [
            {'name': 'libc.so', 'base': '0x7f000000', 'size': 0x200000, 'path': '/lib/libc.so'},
            {'name': 'libm.so', 'base': '0x7f200000', 'size': 0x100000, 'path': '/lib/libm.so'},
            {'name': 'test.exe', 'base': '0x400000', 'size': 0x10000, 'path': '/bin/test.exe'},
        ]

        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'modules', 'modules': test_modules}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool with pattern
        result = find_modules("test123", pattern="lib*.so")

        # Verify
        assert "error" not in result
        assert result["count"] == 2
        assert all(m["name"].endswith(".so") for m in result["modules"])

    @patch('mcp.tools._get_frida')
    def test_resolve_symbol(self, mock_get_frida):
        """Test resolving symbol."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate symbol resolution
        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'symbol', 'address': '0x400500', 'name': 'printf'}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = resolve_symbol("test123", "libc.so", "printf")

        # Verify
        assert "error" not in result
        assert result["name"] == "printf"
        assert result["address"] == "0x400500"
        assert result["module"] == "libc.so"

    @patch('mcp.tools._get_frida')
    def test_resolve_symbol_not_found(self, mock_get_frida):
        """Test resolving non-existent symbol."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate symbol not found
        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'not_found', 'name': 'nonexistent'}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = resolve_symbol("test123", "libc.so", "nonexistent")

        # Verify
        assert "error" in result
        assert "not found" in result["error"]

    @patch('mcp.tools._get_frida')
    def test_execute_script(self, mock_get_frida):
        """Test executing script."""
        # Setup mock session
        mock_session = Mock()
        mock_script = Mock()

        _sessions["test123"] = {
            "frida_session": mock_session,
            "device": Mock(),
            "target_pid": 1234,
            "target_name": "test",
            "created_at": "2024-01-01T00:00:00"
        }

        mock_session.create_script.return_value = mock_script

        # Simulate script output
        def mock_on(event, callback):
            if event == 'message':
                callback({
                    'type': 'send',
                    'payload': {'type': 'output', 'data': 'test output'}
                }, None)

        mock_script.on.side_effect = mock_on

        # Call tool
        result = execute_script("test123", "console.log('test');")

        # Verify
        assert "error" not in result
        assert result["output"] == "test output"
        assert result["return_code"] == 0


class TestServer:
    """Test MCP Server."""

    def test_server_creation(self):
        """Test that server can be created."""
        from mcp.server import create_mcp_server

        server = create_mcp_server()
        assert server is not None
        assert server.name == "frida-mcp-server"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
