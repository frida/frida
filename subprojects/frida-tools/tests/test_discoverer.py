import subprocess
import threading
import time
import unittest

import frida

from frida_tools.discoverer import UI, Discoverer
from frida_tools.reactor import Reactor

from .data import target_program


class TestDiscoverer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.target = subprocess.Popen([target_program], stdin=subprocess.PIPE)
        # TODO: improve injectors to handle injection into a process that hasn't yet finished initializing
        time.sleep(0.05)
        cls.session = frida.attach(cls.target.pid)

    @classmethod
    def tearDownClass(cls):
        cls.session.detach()
        cls.target.terminate()
        cls.target.stdin.close()
        cls.target.wait()

    def test_basics(self):
        test_ui = TestUI()
        reactor = Reactor(lambda reactor: test_ui.on_result.wait())

        def start():
            d = Discoverer(reactor)
            d.start(self.session, "qjs", test_ui)
            reactor.schedule(d.stop, 0.1)

        reactor.schedule(start)
        reactor.run()
        self.assertIsInstance(test_ui.module_functions, dict)
        self.assertIsInstance(test_ui.dynamic_functions, list)


class TestUI(UI):
    def __init__(self):
        super(UI, self).__init__()
        self.module_functions = None
        self.dynamic_functions = None
        self.on_result = threading.Event()

    def on_sample_result(self, module_functions, dynamic_functions):
        self.module_functions = module_functions
        self.dynamic_functions = dynamic_functions
        self.on_result.set()


if __name__ == "__main__":
    unittest.main()
