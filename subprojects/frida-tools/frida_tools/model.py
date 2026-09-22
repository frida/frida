class Module:
    def __init__(self, name: str, base_address: int, size: int, path: str) -> None:
        self.name = name
        self.base_address = base_address
        self.size = size
        self.path = path

    def __repr__(self) -> str:
        return 'Module(name="%s", base_address=0x%x, size=%d, path="%s")' % (
            self.name,
            self.base_address,
            self.size,
            self.path,
        )

    def __hash__(self) -> int:
        return self.base_address.__hash__()

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Module) and self.base_address == other.base_address

    def __ne__(self, other: object) -> bool:
        return not (isinstance(other, Module) and self.base_address == other.base_address)


class Function:
    def __init__(self, name: str, absolute_address: int) -> None:
        self.name = name
        self.absolute_address = absolute_address

    def __str__(self) -> str:
        return self.name

    def __repr__(self) -> str:
        return 'Function(name="%s", absolute_address=0x%x)' % (self.name, self.absolute_address)

    def __hash__(self) -> int:
        return self.absolute_address.__hash__()

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Function) and self.absolute_address == other.absolute_address

    def __ne__(self, other: object) -> bool:
        return not (isinstance(other, Function) and self.absolute_address == other.absolute_address)


class ModuleFunction(Function):
    def __init__(self, module: Module, name: str, relative_address: int, exported: bool) -> None:
        super().__init__(name, module.base_address + relative_address)
        self.module = module
        self.relative_address = relative_address
        self.exported = exported

    def __repr__(self) -> str:
        return 'ModuleFunction(module="%s", name="%s", relative_address=0x%x)' % (
            self.module.name,
            self.name,
            self.relative_address,
        )


class ObjCMethod(Function):
    def __init__(self, mtype: str, cls: str, method: str, address: int) -> None:
        self.mtype = mtype
        self.cls = cls
        self.method = method
        self.address = address
        super().__init__(self.display_name(), address)

    def display_name(self) -> str:
        return "{mtype}[{cls} {method}]".format(mtype=self.mtype, cls=self.cls, method=self.method)

    def __repr__(self) -> str:
        return 'ObjCMethod(mtype="%s", cls="%s", method="%s", address=0x%x)' % (
            self.mtype,
            self.cls,
            self.method,
            self.address,
        )
