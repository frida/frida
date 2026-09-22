import argparse
import threading
from typing import List, Mapping, Optional, Tuple

import frida

from frida_tools.application import ConsoleApplication, await_enter
from frida_tools.model import Function, Module, ModuleFunction
from frida_tools.reactor import Reactor


class UI:
    def on_sample_start(self, total: int) -> None:
        pass

    def on_sample_result(
        self,
        module_functions: Mapping[Module, List[Tuple[ModuleFunction, int]]],
        dynamic_functions: List[Tuple[ModuleFunction, int]],
    ) -> None:
        pass

    def _on_script_created(self, script: frida.core.Script) -> None:
        pass


class Discoverer:
    def __init__(self, reactor: Reactor) -> None:
        self._reactor = reactor
        self._ui = None
        self._script: Optional[frida.core.Script] = None

    def dispose(self) -> None:
        if self._script is not None:
            try:
                self._script.unload()
            except:
                pass
            self._script = None

    def start(self, session: frida.core.Session, runtime: str, ui: UI) -> None:
        def on_message(message, data) -> None:
            print(message, data)

        self._ui = ui

        script = session.create_script(name="discoverer", source=self._create_discover_script(), runtime=runtime)
        self._script = script
        self._ui._on_script_created(script)
        script.on("message", on_message)
        script.load()

        params = script.exports_sync.start()
        ui.on_sample_start(params["total"])

    def stop(self) -> None:
        result = self._script.exports_sync.stop()

        modules = {
            int(module_id): Module(m["name"], int(m["base"], 16), m["size"], m["path"])
            for module_id, m in result["modules"].items()
        }

        module_functions = {}
        dynamic_functions = []
        for module_id, name, visibility, raw_address, count in result["targets"]:
            address = int(raw_address, 16)

            if module_id != 0:
                module = modules[module_id]
                exported = visibility == "e"
                function = ModuleFunction(module, name, address - module.base_address, exported)

                functions = module_functions.get(module, [])
                if len(functions) == 0:
                    module_functions[module] = functions
                functions.append((function, count))
            else:
                function = Function(name, address)

                dynamic_functions.append((function, count))

        self._ui.on_sample_result(module_functions, dynamic_functions)

    def _create_discover_script(self) -> str:
        return """\
const threadIds = new Set();
const result = new Map();

rpc.exports = {
    start: function () {
        for (const { id: threadId } of Process.enumerateThreads()) {
            threadIds.add(threadId);
            Stalker.follow(threadId, {
                events: { call: true },
                onCallSummary(summary) {
                    for (const [address, count] of Object.entries(summary)) {
                        result.set(address, (result.get(address) ?? 0) + count);
                    }
                }
            });
        }

        return {
            total: threadIds.size
        };
    },
    stop: function () {
        for (const threadId of threadIds.values()) {
            Stalker.unfollow(threadId);
        }
        threadIds.clear();

        const targets = [];
        const modules = {};

        const moduleMap = new ModuleMap();
        const allModules = moduleMap.values().reduce((m, module) => m.set(module.path, module), new Map());
        const moduleDetails = new Map();
        let nextModuleId = 1;

        for (const [address, count] of result.entries()) {
            let moduleId = 0;
            let name;
            let visibility = 'i';
            const addressPtr = ptr(address);

            const path = moduleMap.findPath(addressPtr);
            if (path !== null) {
                const module = allModules.get(path);

                let details = moduleDetails.get(path);
                if (details !== undefined) {
                    moduleId = details.id;
                } else {
                    moduleId = nextModuleId++;

                    details = {
                        id: moduleId,
                        exports: module.enumerateExports().reduce((m, e) => m.set(e.address.toString(), e.name), new Map())
                    };
                    moduleDetails.set(path, details);

                    modules[moduleId] = module;
                }

                const exportName = details.exports.get(address);
                if (exportName !== undefined) {
                    name = exportName;
                    visibility = 'e';
                } else {
                    name = 'sub_' + addressPtr.sub(module.base).toString(16);
                }
            } else {
                name = 'dsub_' + addressPtr.toString(16);
            }

            targets.push([moduleId, name, visibility, address, count]);
        }

        result.clear();

        return {
            targets,
            modules
        };
    }
};
"""


class DiscovererApplication(ConsoleApplication, UI):
    _discoverer: Optional[Discoverer]

    def __init__(self) -> None:
        self._results_received = threading.Event()
        ConsoleApplication.__init__(self, self._await_keys)

    def _await_keys(self, reactor: Reactor) -> None:
        await_enter(reactor)
        reactor.schedule(lambda: self._discoverer.stop())
        while reactor.is_running() and not self._results_received.is_set():
            self._results_received.wait(0.5)

    def _usage(self) -> str:
        return "%(prog)s [options] target"

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        self._discoverer = None

    def _needs_target(self) -> bool:
        return True

    def _start(self) -> None:
        self._update_status("Injecting script...")
        self._discoverer = Discoverer(self._reactor)
        self._discoverer.start(self._session, self._runtime, self)

    def _stop(self) -> None:
        self._print("Stopping...")
        assert self._discoverer is not None
        self._discoverer.dispose()
        self._discoverer = None

    def on_sample_start(self, total: int) -> None:
        self._update_status(f"Tracing {total} threads. Press ENTER to stop.")
        self._resume()

    def on_sample_result(
        self,
        module_functions: Mapping[Module, List[Tuple[ModuleFunction, int]]],
        dynamic_functions: List[Tuple[ModuleFunction, int]],
    ) -> None:
        for module, functions in module_functions.items():
            self._print(module.name)
            self._print("\t%-10s\t%s" % ("Calls", "Function"))
            for function, count in sorted(functions, key=lambda item: item[1], reverse=True):
                self._print("\t%-10d\t%s" % (count, function))
            self._print("")

        if len(dynamic_functions) > 0:
            self._print("Dynamic functions:")
            self._print("\t%-10s\t%s" % ("Calls", "Function"))
            for function, count in sorted(dynamic_functions, key=lambda item: item[1], reverse=True):
                self._print("\t%-10d\t%s" % (count, function))

        self._results_received.set()


def main() -> None:
    app = DiscovererApplication()
    app.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
