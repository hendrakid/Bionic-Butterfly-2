from queue import Empty
import time

from link_monitor.serial_worker import MAX_LINE_BYTES, SerialWorker


class FakeSerial:
    def __init__(self, chunks, **kwargs):
        self.chunks = iter(chunks)
        self.settings = kwargs
        self.opened = False
        self.closed = False
        self.dtr = True
        self.rts = True

    def open(self):
        assert self.dtr is False and self.rts is False
        assert self.settings["port"] is None
        self.opened = True

    def read(self, size):
        try:
            return next(self.chunks)
        except StopIteration:
            time.sleep(0.01)
            return b""

    def write(self, payload):
        raise AssertionError("Monitor must never write serial payloads")

    def close(self):
        self.closed = True


def await_messages(worker, count):
    messages = []
    deadline = time.monotonic() + 2
    while len(messages) < count and time.monotonic() < deadline:
        try:
            messages.append(worker.messages.get(timeout=0.05))
        except Empty:
            pass
    return messages


def test_partial_lines_two_workers_and_shutdown():
    devices = []
    def factory(**kwargs):
        device = FakeSerial([b"CTRL t=1 con", b"trols=1\r\nsecond\n"], **kwargs)
        devices.append(device)
        return device
    workers = [SerialWorker("CTRL", "COM4", factory=factory),
               SerialWorker("RX", "COM10", factory=factory)]
    try:
        for worker in workers:
            worker.start()
        for worker in workers:
            assert [m.raw for m in await_messages(worker, 2)] == ["CTRL t=1 controls=1", "second"]
    finally:
        for worker in workers:
            worker.close()
    assert all(d.closed for d in devices)
    assert all(w.error is None for w in workers)


def test_oversized_line_discard_recovers_at_newline():
    device = None
    def factory(**kwargs):
        nonlocal device
        device = FakeSerial([b"x" * (MAX_LINE_BYTES + 10), b"\nvalid\n"], **kwargs)
        return device
    worker = SerialWorker("RX", "COM10", factory=factory)
    worker.start()
    try:
        messages = await_messages(worker, 2)
        assert messages[0].diagnostic
        assert messages[1].raw == "valid"
    finally:
        worker.close()


def test_open_failure_and_overflow_are_visible():
    def factory(**kwargs):
        raise OSError("port busy")
    worker = SerialWorker("CTRL", "COM4", factory=factory)
    worker.start()
    worker.close()
    assert "port busy" in worker.error
    for _ in range(worker.messages.maxsize):
        worker._emit("line")
    assert not worker._emit("lost")
    assert "overflow" in worker.error
