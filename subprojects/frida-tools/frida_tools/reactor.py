import collections
import threading
import time
from typing import Callable, Deque, Optional, Tuple, Union

import frida


class Reactor:
    """
    Run the given function until return in the main thread (or the thread of
    the run method) and in a background thread receive and run additional tasks.
    """

    def __init__(
        self, run_until_return: Callable[["Reactor"], None], on_stop: Optional[Callable[[], None]] = None
    ) -> None:
        self._running = False
        self._run_until_return = run_until_return
        self._on_stop = on_stop
        self._pending: Deque[Tuple[Callable[[], None], Union[int, float]]] = collections.deque([])
        self._lock = threading.Lock()
        self._cond = threading.Condition(self._lock)

        self.io_cancellable = frida.Cancellable()

        self.ui_cancellable = frida.Cancellable()
        self._ui_cancellable_fd = self.ui_cancellable.get_pollfd()

    def __del__(self) -> None:
        self._ui_cancellable_fd.release()

    def is_running(self) -> bool:
        with self._lock:
            return self._running

    def run(self) -> None:
        with self._lock:
            self._running = True

        worker = threading.Thread(target=self._run)
        worker.start()

        self._run_until_return(self)

        self.stop()
        worker.join()

    def _run(self) -> None:
        running = True
        while running:
            now = time.time()
            work = None
            timeout = None
            previous_pending_length = -1
            with self._lock:
                for item in self._pending:
                    f, when = item
                    if now >= when:
                        work = f
                        self._pending.remove(item)
                        break
                if len(self._pending) > 0:
                    timeout = max([min(map(lambda item: item[1], self._pending)) - now, 0])
                previous_pending_length = len(self._pending)

            if work is not None:
                with self.io_cancellable:
                    try:
                        work()
                    except frida.OperationCancelledError:
                        pass

            with self._lock:
                if self._running and len(self._pending) == previous_pending_length:
                    self._cond.wait(timeout)
                running = self._running

        if self._on_stop is not None:
            self._on_stop()

        self.ui_cancellable.cancel()

    def stop(self) -> None:
        self.schedule(self._stop)

    def _stop(self) -> None:
        with self._lock:
            self._running = False

    def schedule(self, f: Callable[[], None], delay: Optional[Union[int, float]] = None) -> None:
        """
        append a function to the tasks queue of the reactor, optionally with a
        delay in seconds
        """

        now = time.time()
        if delay is not None:
            when = now + delay
        else:
            when = now
        with self._lock:
            self._pending.append((f, when))
            self._cond.notify()

    def cancel_io(self) -> None:
        self.io_cancellable.cancel()
