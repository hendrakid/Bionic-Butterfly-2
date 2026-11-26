from pathlib import Path
from queue import Queue

import pytest

from link_monitor.serial_worker import SerialMessage
from link_monitor.session import MonitorSession

CTRL = "CTRL t={t} controls={c} confirmed_acks={a} ack_rtt_ms=3 confirmed=1 radio_ok=5 radio_fail=0 rx_overflow=0 rx_high=1 ack_rejected=0 send_fail=0 missed_slots=0"
RX = "RX t={t} controls={c} rejected=0 expired=0 acks={c} ack_fail=0 timed_out=0 radio_ok=5 radio_fail=0 rx_overflow=0 rx_high=1 timeouts=0 rx_frames={c} rx_invalid=0 rx_oversized=0 future=0"


class FakeWorker:
    def __init__(self, role, port):
        self.role, self.port = role, port
        self.connected = False
        self.error = None
        self.messages = Queue()
        self.closed = False

    def start(self):
        self.connected = True

    def close(self):
        self.closed = True
        self.connected = False


class FakeSink:
    def __init__(self, path, metadata):
        self.records = []
        self.error = None
        self.summary = None

    def submit(self, record):
        self.records.append(record)

    def close(self, summary):
        self.summary = summary


def connected_session():
    clock = [10.0]
    session = MonitorSession(FakeWorker, FakeSink, lambda: clock[0])
    session.connect("CTRL", "COM4")
    session.connect("RX", "COM10")
    send(session, clock, 1000, 10, 10)
    return session, clock


def send(session, clock, t, c, a):
    for role, raw in [("CTRL", CTRL.format(t=t, c=c, a=a)), ("RX", RX.format(t=t, c=c))]:
        session.workers[role].messages.put(SerialMessage(raw, clock[0], "2026-01-01T00:00:00Z"))
    session.poll()


def test_start_fresh_baseline_stop_preserves_ports_and_metrics():
    session, clock = connected_session()
    assert session.can_start()
    session.start(Path("unused.jsonl"))
    sink = session._sink
    assert not session.snapshot()["roles"]["CTRL"]["ready"]
    clock[0] += 1
    send(session, clock, 2000, 20, 19)
    clock[0] += 1
    send(session, clock, 3000, 30, 28)
    assert session.snapshot()["roles"]["CTRL"]["totals"]["controls"] == 10
    session.stop()
    snapshot = session.snapshot()
    clock[0] += 1
    send(session, clock, 4000, 40, 38)
    assert session.snapshot() == snapshot
    assert len(sink.records) == 4
    assert not sink.summary["incomplete"]
    assert all(w.connected and not w.closed for w in session.workers.values())
    session.start(Path("new.jsonl"))
    assert not session.snapshot()["roles"]["CTRL"]["ready"]
    session.close()


def test_port_role_stale_and_disconnect_guards():
    session, clock = connected_session()
    with pytest.raises(ValueError):
        session.connect("RX", "com4")
    session.workers["CTRL"].messages.put(SerialMessage(RX.format(t=1000, c=10), clock[0], "now"))
    session.poll()
    assert not session.can_start()
    send(session, clock, 2000, 20, 20)
    clock[0] += 4
    assert not session.can_start()
    send(session, clock, 3000, 30, 30)
    session.start(Path("unused.jsonl"))
    session.workers["RX"].error = "USB disconnected"
    session.poll()
    assert not session.active and session.incomplete
    assert "USB disconnected" in session.snapshot()["quality"]
    session.close()


def test_real_storage_flushes_capture_and_replays_frozen_metrics(tmp_path):
    from link_monitor.metrics import MetricsEngine
    from link_monitor.replay import iter_records
    from link_monitor.session import CaptureSink

    clock = [10.0]
    session = MonitorSession(FakeWorker, CaptureSink, lambda: clock[0])
    session.connect("CTRL", "COM4")
    session.connect("RX", "COM10")
    send(session, clock, 1000, 10, 10)
    path = tmp_path / "capture.jsonl"
    session.start(path)
    clock[0] += 1
    send(session, clock, 2000, 20, 19)
    clock[0] += 1
    send(session, clock, 3000, 30, 28)
    session.stop()
    captured = path.read_bytes()
    clock[0] += 1
    send(session, clock, 4000, 40, 38)
    assert path.read_bytes() == captured
    records = list(iter_records(path))
    assert len([record for record in records if record.kind == "stats"]) == 4
    engine = MetricsEngine()
    for record in records:
        engine.ingest(record)
    replay = engine.snapshot()["roles"]["CTRL"]
    live = session.snapshot()["roles"]["CTRL"]
    assert replay["totals"] == live["totals"]
    assert replay["ack_coverage"] == live["ack_coverage"] == 90.0
    assert not session.incomplete
    session.close()


def test_failed_start_preserves_previous_capture(tmp_path):
    session, clock = connected_session()
    session.start(tmp_path / "previous.jsonl")
    clock[0] += 1
    send(session, clock, 2000, 20, 19)
    clock[0] += 1
    send(session, clock, 3000, 30, 28)
    session.stop()
    previous = session.snapshot()
    previous_path = session.path

    def fail_sink(path, metadata):
        raise OSError("permission denied")

    session._sink_factory = fail_sink
    with pytest.raises(OSError, match="permission denied"):
        session.start(tmp_path / "failed.jsonl")
    assert session.snapshot() == previous
    assert session.path == previous_path
    assert not session.active
    session.close()


def test_closed_storage_rejects_new_records(tmp_path):
    from link_monitor.models import LogRecord
    from link_monitor.session import CaptureSink

    path = tmp_path / "capture.jsonl"
    sink = CaptureSink(path, {})
    sink.close({"incomplete": False})
    saved = path.read_bytes()
    with pytest.raises(OSError, match="already closed"):
        sink.submit(LogRecord("CTRL", "late", 1, "now"))
    sink.close({"incomplete": True})
    assert path.read_bytes() == saved


def test_pre_start_queued_lines_are_not_captured_and_storage_failure_stops():
    session, clock = connected_session()
    session.workers["CTRL"].messages.put(SerialMessage(CTRL.format(t=1100, c=11, a=11), 9, "old"))
    session.start(Path("unused.jsonl"))
    sink = session._sink
    session.poll()
    assert not sink.records
    sink.error = "disk full"
    session.poll()
    assert session.incomplete and not session.active
    session.close()
