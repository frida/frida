def main() -> None:
    import functools
    import threading

    import frida
    from prompt_toolkit.application import Application
    from prompt_toolkit.key_binding import KeyBindings
    from prompt_toolkit.layout.containers import HSplit, VSplit
    from prompt_toolkit.layout.layout import Layout
    from prompt_toolkit.widgets import Label

    from frida_tools.application import ConsoleApplication
    from frida_tools.reactor import Reactor

    class LSDApplication(ConsoleApplication):
        def __init__(self) -> None:
            super().__init__(self._process_input, self._on_stop)
            self._ui_app = None
            self._pending_labels = set()
            self._spinner_frames = ["v", "<", "^", ">"]
            self._spinner_offset = 0
            self._lock = threading.Lock()

        def _usage(self) -> str:
            return "%(prog)s [options]"

        def _needs_device(self) -> bool:
            return False

        def _process_input(self, reactor: Reactor) -> None:
            try:
                devices = frida.enumerate_devices()
            except Exception as e:
                self._update_status(f"Failed to enumerate devices: {e}")
                self._exit(1)
                return

            bindings = KeyBindings()

            @bindings.add("<any>")
            def _(event):
                self._reactor.io_cancellable.cancel()

            self._ui_app = Application(key_bindings=bindings, full_screen=False)

            id_rows = []
            type_rows = []
            name_rows = []
            os_rows = []
            for device in sorted(devices, key=functools.cmp_to_key(compare_devices)):
                id_rows.append(Label(device.id, dont_extend_width=True))
                type_rows.append(Label(device.type, dont_extend_width=True))
                name_rows.append(Label(device.name, dont_extend_width=True))
                os_label = Label("", dont_extend_width=True)
                os_rows.append(os_label)

                with self._lock:
                    self._pending_labels.add(os_label)
                worker = threading.Thread(target=self._fetch_parameters, args=(device, os_label))
                worker.start()

            status_label = Label(" ")
            body = HSplit(
                [
                    VSplit(
                        [
                            HSplit([Label("Id", dont_extend_width=True), HSplit(id_rows)], padding_char="-", padding=1),
                            HSplit(
                                [Label("Type", dont_extend_width=True), HSplit(type_rows)], padding_char="-", padding=1
                            ),
                            HSplit(
                                [Label("Name", dont_extend_width=True), HSplit(name_rows)], padding_char="-", padding=1
                            ),
                            HSplit([Label("OS", dont_extend_width=True), HSplit(os_rows)], padding_char="-", padding=1),
                        ],
                        padding=2,
                    ),
                    status_label,
                ]
            )

            self._ui_app.layout = Layout(body, focused_element=status_label)

            self._reactor.schedule(self._update_progress)
            self._ui_app.run()
            self._ui_app._redraw()

        def _on_stop(self):
            if self._ui_app is not None:
                self._ui_app.exit()

        def _update_progress(self):
            with self._lock:
                if not self._pending_labels:
                    self._exit(0)
                    return

                glyph = self._spinner_frames[self._spinner_offset % len(self._spinner_frames)]
                self._spinner_offset += 1
                for label in self._pending_labels:
                    label.text = glyph
            self._ui_app.invalidate()

            self._reactor.schedule(self._update_progress, delay=0.1)

        def _fetch_parameters(self, device, os_label):
            try:
                with self._reactor.io_cancellable:
                    params = device.query_system_parameters()
                os = params["os"]
                version = os.get("version")
                if version is not None:
                    text = os["name"] + " " + version
                else:
                    text = os["name"]
            except:
                text = ""

            with self._lock:
                os_label.text = text
                self._pending_labels.remove(os_label)

            self._ui_app.invalidate()

    def compare_devices(a: frida.core.Device, b: frida.core.Device) -> int:
        a_score = score(a)
        b_score = score(b)
        if a_score == b_score:
            if a.name is None or b.name is None:
                return 0
            if a.name > b.name:
                return 1
            elif a.name < b.name:
                return -1
            else:
                return 0
        else:
            if a_score > b_score:
                return -1
            elif a_score < b_score:
                return 1
            else:
                return 0

    def score(device: frida.core.Device) -> int:
        type = device.type
        if type == "local":
            return 3
        elif type == "usb":
            return 2
        else:
            return 1

    app = LSDApplication()
    app.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
