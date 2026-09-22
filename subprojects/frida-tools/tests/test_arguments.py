import unittest

from frida_tools.application import ConsoleApplication
from frida_tools.kill import KillApplication


class DummyConsoleApplication(ConsoleApplication):
    def _usage(self):
        return "no usage"


class DeviceParsingTestCase(unittest.TestCase):
    def test_short_device_id(self):
        test_cases = [("short device id", "123", ["-D", "123"]), ("long device id", "abc", ["--device", "abc"])]
        for message, result, args in test_cases:
            with self.subTest(message, args=args):
                app = DummyConsoleApplication(args=args)
                self.assertEqual(result, app._device_id)

    def test_device_id_missing(self):
        test_cases = [("short device", ["-D"]), ("long device", ["--device"])]
        for message, args in test_cases:
            with self.subTest(message, args=args):
                with self.assertRaises(SystemExit):
                    DummyConsoleApplication(args=args)

    def test_device_type(self):
        test_cases = [
            ("short usb", "usb", ["-U"]),
            ("long usb", "usb", ["--usb"]),
            ("short remote", "remote", ["-R"]),
            ("long remote", "remote", ["--remote"]),
        ]
        for message, result, args in test_cases:
            with self.subTest(message, args=args):
                app = DummyConsoleApplication(args=args)
                self.assertEqual(app._device_type, result)

    def test_remote_host(self):
        test_cases = [
            ("short host", "127.0.0.1", ["-H", "127.0.0.1"]),
            ("long host", "192.168.1.1:1234", ["--host", "192.168.1.1:1234"]),
        ]

        for message, result, args in test_cases:
            with self.subTest(message, args=args):
                app = DummyConsoleApplication(args=args)
                self.assertEqual(app._host, result)

    def test_missing_remote_host(self):
        test_cases = [("short host", ["-H"]), ("long host", ["--host"])]
        for message, args in test_cases:
            with self.subTest(message, args=args):
                with self.assertRaises(SystemExit):
                    DummyConsoleApplication(args=args)

    def test_certificate(self):
        path = "/path/to/file"
        args = ["--certificate", path]
        app = DummyConsoleApplication(args=args)
        self.assertEqual(path, app._certificate)

    def test_missing_certificate(self):
        args = ["--certificate"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_origin(self):
        origin = "null"
        args = ["--origin", origin]
        app = DummyConsoleApplication(args=args)
        self.assertEqual(origin, app._origin)

    def test_missing_origin(self):
        args = ["--origin"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_token(self):
        token = "ABCDEF"
        args = ["--token", token]
        app = DummyConsoleApplication(args=args)
        self.assertEqual(token, app._token)

    def test_missing_token(self):
        args = ["--token"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_keepalive_interval(self):
        interval = 123
        args = ["--keepalive-interval", str(interval)]
        app = DummyConsoleApplication(args=args)
        self.assertEqual(interval, app._keepalive_interval)

    def test_missing_keepalive_interval(self):
        args = ["--keepalive-interval"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_non_decimal_keepalive_interval(self):
        args = ["--keepalive-interval", "abc"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_default_session_transport(self):
        app = DummyConsoleApplication(args=[])
        self.assertEqual("multiplexed", app._session_transport)

    def test_p2p_session_transport(self):
        app = DummyConsoleApplication(args=["--p2p"])
        self.assertEqual("p2p", app._session_transport)

    def test_stun_server(self):
        stun_server = "192.168.1.1"
        args = ["--stun-server", stun_server]
        app = DummyConsoleApplication(args=args)
        self.assertEqual(stun_server, app._stun_server)

    def test_missing_stun_server(self):
        args = ["--stun-server"]
        with self.assertRaises(SystemExit):
            DummyConsoleApplication(args=args)

    def test_single_relay(self):
        address = "127.0.0.1"
        username = "admin"
        password = "password"
        kind = "turn-udp"

        serialized = ",".join((address, username, password, kind))
        args = ["--relay", serialized]
        app = DummyConsoleApplication(args=args)

        self.assertEqual(len(app._relays), 1)
        self.assertEqual(app._relays[0].address, address)
        self.assertEqual(app._relays[0].username, username)
        self.assertEqual(app._relays[0].password, password)
        self.assertEqual(app._relays[0].kind, kind)

    def test_multiple_relay(self):
        relays = [("127.0.0.1", "admin", "password", "turn-udp"), ("192.168.1.1", "user", "user", "turn-tls")]
        args = []
        for relay in relays:
            args.append("--relay")
            args.append(",".join(relay))

        app = DummyConsoleApplication(args=args)

        self.assertEqual(len(app._relays), len(relays))
        for i in range(len(relays)):
            self.assertEqual(app._relays[i].address, relays[i][0])
            self.assertEqual(app._relays[i].username, relays[i][1])
            self.assertEqual(app._relays[i].password, relays[i][2])
            self.assertEqual(app._relays[i].kind, relays[i][3])

    def test_multiple_device_types(self):
        combinations = [("host and device id", ["--host", "127.0.0.1", "-D", "ABCDEF"])]

        for message, args in combinations:
            with self.subTest(message, args=args):
                with self.assertRaises(SystemExit):
                    DummyConsoleApplication(args=args)


class KillParsingTestCase(unittest.TestCase):
    def test_no_arguments(self):
        with self.assertRaises(SystemExit):
            KillApplication(args=[])

    def test_passing_pid(self):
        kill_app = KillApplication(args=["2"])
        self.assertEqual(kill_app._process, 2)

    def test_passing_process_name(self):
        kill_app = KillApplication(args=["python"])
        self.assertEqual(kill_app._process, "python")

    def test_passing_file(self):
        with self.assertRaises(SystemExit):
            KillApplication(args=["./file"])
