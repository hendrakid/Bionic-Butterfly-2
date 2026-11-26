from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from queue import Empty, Full, Queue
import threading
import time
from typing import Callable

from .metrics import MetricsEngine
from .models import LogRecord
from .parser import parse_line
from .serial_worker import SerialWorker
from .storage import SessionWriter

LOG_STALE_SECONDS = 3.0


class CaptureSink:
    def __init__(self, path: Path, metadata: dict):
        self._writer = SessionWriter(path, metadata)
        self._queue: Queue = Queue(2048)
        self._stop = threading.Event()
        self.error: str | None = None
        self.summary: dict = {}
        self._thread = threading.Thread(target=self._run, name="capture-storage", daemon=True)
        self._thread.start()

    def submit(self, record: LogRecord) -> None:
        if self._stop.is_set():
            raise OSError("Capture storage already closed")
        if self.error:
            raise OSError(self.error)
        try:
            self._queue.put_nowait(record)
        except Full as exc:
            self.error = "Storage backlog overflow: capture incomplete"
            raise OSError(self.error) from exc

    def close(self, summary: dict) -> None:
        if self._stop.is_set():
            return
        self.summary = summary
        self._stop.set()
        self._thread.join(3)
        if self._thread.is_alive():
            self.error = self.error or "Storage shutdown timed out; file may be partial"

    def _run(self) -> None:
        try:
            while not self._stop.is_set() or not self._queue.empty():
                try:
                    record = self._queue.get(timeout=0.1)
                except Empty:
                    continue
                self._writer.write(record)
        except (OSError, ValueError) as exc:
            self.error = f"Capture storage failed: {exc}"
        finally:
            try:
                summary = dict(self.summary)
                if self.error:
                    summary.update(incomplete=True, storage_error=self.error)
                self._writer.close(summary)
            except (OSError, ValueError) as exc:
                self.error = self.error or f"Capture close failed: {exc}"


class MonitorSession:
    def __init__(self, worker_factory: Callable = SerialWorker,
                 sink_factory: Callable = CaptureSink, clock: Callable = time.monotonic):
        self.workers: dict[str, SerialWorker] = {}
        self.verified: dict[str, bool] = {"CTRL": False, "RX": False}
        self.last_log: dict[str, float] = {}
        self.last_stats: dict[str, float] = {}
        self.engine = MetricsEngine()
        self.active = False
        self.incomplete = False
        self.quality: list[str] = []
        self.path: Path | None = None
        self._worker_factory = worker_factory
        self._sink_factory = sink_factory
        self._clock = clock
        self._start_at = clock()
        self._sink: CaptureSink | None = None
        self._reported_errors: dict[str, str] = {}

    def connect(self, role: str, port: str) -> None:
        if self.active:
            raise ValueError("Stop capture before changing ports")
        if role not in self.verified:
            raise ValueError("Unknown role")
        if role in self.workers:
            raise ValueError("Disconnect this role before connecting again")
        if any(worker.port.casefold() == port.casefold() for worker in self.workers.values()):
            raise ValueError("Controller and Receiver must use different ports")
        if not port:
            raise ValueError("Choose a serial port")
        worker = self._worker_factory(role, port)
        self.workers[role] = worker
        self.verified[role] = False
        self._reported_errors.pop(role, None)
        worker.start()

    def disconnect(self, role: str) -> None:
        if self.active:
            self.stop("Port disconnected during capture")
        worker = self.workers.pop(role, None)
        if worker:
            worker.close()
            if worker.error:
                self._flag(worker.error)
        self.verified[role] = False
        self.last_log.pop(role, None)
        self.last_stats.pop(role, None)

    def can_start(self) -> bool:
        now = self._clock()
        return not self.active and all(
            role in self.workers and self.workers[role].connected
            and not self.workers[role].error and self.verified[role]
            and now - self.last_stats.get(role, float("-inf")) < LOG_STALE_SECONDS
            for role in ("CTRL", "RX")
        )

    def start(self, path: Path) -> None:
        if not self.can_start():
            raise ValueError("Connect both roles and wait for fresh recognized CTRL/RX stats")
        started_at = self._clock()
        metadata = {
            "started_at": datetime.now(timezone.utc).isoformat(),
            "ports": {role: worker.port for role, worker in self.workers.items()},
            "baud": 115200,
            "observation": "aggregate counters; ACK coverage is an estimate",
            "baseline": "first fresh stats record of each role after Start",
        }
        sink = self._sink_factory(path, metadata)
        self.engine.reset()
        self.quality.clear()
        self.incomplete = False
        self._start_at = started_at
        self._sink = sink
        self.path = path
        self.active = True

    def _flag(self, message: str) -> None:
        if message not in self.quality:
            self.quality.append(message)

    def poll(self) -> list[LogRecord]:
        records = []
        for role, worker in self.workers.items():
            for _ in range(256):
                try:
                    message = worker.messages.get_nowait()
                except Empty:
                    break
                self.last_log[role] = message.received_at
                record = parse_line(role, message.raw, max(0.0, message.received_at - self._start_at),
                                    message.wall_time)
                if message.diagnostic:
                    record.kind = "malformed"
                if record.kind == "stats":
                    self.verified[role] = True
                    self.last_stats[role] = message.received_at
                elif record.kind in ("mismatch", "boot"):
                    self.verified[role] = False
                records.append(record)
                if self.active and message.received_at >= self._start_at:
                    if message.diagnostic or record.kind == "mismatch":
                        self.incomplete = True
                        self._flag(f"{role}: {message.raw}")
                    self.engine.ingest(record)
                    try:
                        self._sink.submit(record)
                    except OSError as exc:
                        self.stop(str(exc))
            if worker.error and self._reported_errors.get(role) != worker.error:
                self._reported_errors[role] = worker.error
                self._flag(worker.error)
                if self.active:
                    self.stop(worker.error)
        if self.active and self._sink.error:
            self.stop(self._sink.error)
        if self.active:
            for role in ("CTRL", "RX"):
                if self._clock() - self.last_stats.get(role, self._start_at) >= LOG_STALE_SECONDS:
                    self.stop(f"{role}: serial stats stale; radio status is unknown")
                    break
        return records

    def snapshot(self) -> dict:
        snapshot = self.engine.snapshot()
        snapshot["quality"] = list(dict.fromkeys(snapshot.get("quality", []) + self.quality))
        snapshot["incomplete"] = self.incomplete
        return snapshot

    def stop(self, reason: str | None = None) -> None:
        if not self.active:
            return
        self.active = False
        if reason:
            self.incomplete = True
            self._flag(reason)
        if self._sink:
            self._sink.close({"incomplete": self.incomplete, "quality": self.quality,
                              "elapsed": self._clock() - self._start_at,
                              "capture_boundary": "aggregate snapshots; outstanding ACKs unknown"})
            if self._sink.error:
                self.incomplete = True
                self._flag(self._sink.error)
            self._sink = None

    def close(self) -> None:
        self.stop()
        for role in tuple(self.workers):
            self.disconnect(role)
