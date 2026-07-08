"""
Data models for Frida MCP Server

These models define the structured output format for MCP tools,
ensuring consistent JSON responses for AI Agent consumption.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional


@dataclass
class Process:
    """Process information."""
    pid: int
    name: str
    path: str = ""
    parameters: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pid": self.pid,
            "name": self.name,
            "path": self.path,
            "parameters": self.parameters,
        }


@dataclass
class Session:
    """Session information."""
    id: str
    target_pid: int
    target_name: str = ""
    status: str = "attached"  # attached, detached, lost
    device_id: str = "local"
    created_at: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "target_pid": self.target_pid,
            "target_name": self.target_name,
            "status": self.status,
            "device_id": self.device_id,
            "created_at": self.created_at,
        }


@dataclass
class Hook:
    """Hook information."""
    id: str
    address: int
    type: str  # "interceptor", "stalker", "breakpoint"
    callback: str = ""
    module: str = ""
    symbol: str = ""
    enabled: bool = True

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "address": hex(self.address),
            "type": self.type,
            "callback": self.callback,
            "module": self.module,
            "symbol": self.symbol,
            "enabled": self.enabled,
        }


@dataclass
class MemoryRegion:
    """Memory region information."""
    base: int
    size: int
    protection: str  # "r--", "rw-", "r-x", "rwx"
    type: str = ""  # "code", "data", "heap", "stack", "module"
    file: str = ""
    file_offset: int = 0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "base": hex(self.base),
            "size": self.size,
            "end": hex(self.base + self.size),
            "protection": self.protection,
            "type": self.type,
            "file": self.file,
            "file_offset": hex(self.file_offset),
        }


@dataclass
class Module:
    """Loaded module information."""
    name: str
    base: int
    size: int
    path: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "base": hex(self.base),
            "size": self.size,
            "end": hex(self.base + self.size),
            "path": self.path,
        }


@dataclass
class Symbol:
    """Symbol information."""
    name: str
    address: int
    size: int = 0
    type: str = ""  # "function", "object", "unknown"
    module: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "address": hex(self.address),
            "size": self.size,
            "type": self.type,
            "module": self.module,
        }


@dataclass
class MemoryContent:
    """Memory content with hex and ASCII representation."""
    address: int
    data: bytes
    ascii_repr: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "address": hex(self.address),
            "hex": self.data.hex(),
            "ascii": self.ascii_repr,
            "size": len(self.data),
        }


@dataclass
class ScriptResult:
    """Script execution result."""
    output: str = ""
    error: str = ""
    return_code: int = 0
    logs: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "output": self.output,
            "error": self.error,
            "return_code": self.return_code,
            "logs": self.logs,
        }


@dataclass
class CommandResult:
    """Command execution result."""
    output: str = ""
    error: str = ""
    return_code: int = 0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "output": self.output,
            "error": self.error,
            "return_code": self.return_code,
        }
