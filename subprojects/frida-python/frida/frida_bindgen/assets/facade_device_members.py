@property
def type(self) -> str:
    return self.dtype

def get_bus(self) -> "Bus":
    return self.bus

def get_process(self, process_name: str) -> "Process":
    process_name_lc = process_name.lower()
    matching = [
        process
        for process in self.enumerate_processes()
        if fnmatch.fnmatchcase(process.name.lower(), process_name_lc)
    ]
    if len(matching) == 1:
        return matching[0]
    if len(matching) > 1:
        matches = ", ".join(f"{p.name} (pid: {p.pid})" for p in matching)
        raise _frida.ProcessNotFoundError(f"ambiguous name; it matches: {matches}")
    raise _frida.ProcessNotFoundError(f"unable to find process with name '{process_name}'")

def _pid_of(self, target: ProcessTarget) -> int:
    if isinstance(target, str):
        return self.get_process(target).pid
    return target
