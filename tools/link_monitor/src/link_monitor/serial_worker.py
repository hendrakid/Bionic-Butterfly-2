from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from queue import Full, Queue
import threading
import time
from typing import Callable

import serial
from serial.tools import list_ports

MAX_LINE_BYTES = 4096
QUEUE_CAPACITY = 1024
READ_TIMEOUT = 0.1


@dataclass(frozen=True)
class SerialMessage:
    raw: str
    received_at: float
    wall_time: str
    diagnostic: bool = False


def available_ports() -> list[tuple[str, str]]:
    return [(port.device, port.description) for port in list_ports.comports()]


class SerialWorker:
    def __init__(self, role: str, port: str, baud: int = 115200,
                 factory: Callable = serial.Serial):
        self.role = role
        self.port = port
        self.baud = baud
        self.messages: Queue[SerialMessage] = Queue(QUEUE_CAPACITY)
        self.error: str | None = None
        self.connected = False
        self._factory = factory
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            raise RuntimeError("Serial worker already started")
        self._thread = threading.Thread(target=self._run, name=f"serial-{self.role}", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(2)
            if self._thread.is_alive():
                self.error = "Serial worker shutdown timed out"

    def _emit(self, raw: str, diagnostic: bool = False) -> bool:
        message = SerialMessage(raw, time.monotonic(), datetime.now(timezone.utc).isoformat(), diagnostic)
        try:
            self.messages.put_nowait(message)
            return True
        except Full:
            self.error = "Serial ingest overflow: capture incomplete"
            self._stop.set()
            return False

    def _run(self) -> None:
        connection = None
        try:
            # Configure before opening; some USB drivers can still pulse lines at open.
            connection = self._factory(port=None, baudrate=self.baud, timeout=READ_TIMEOUT,
                                       write_timeout=READ_TIMEOUT)
            connection.dtr = False
            connection.rts = False
            connection.port = self.port
            connection.open()
            self.connected = True
            buffer = bytearray()
            discarding = False
            while not self._stop.is_set():
                chunk = connection.read(256)
                for byte in chunk:
                    if byte == 10:
                        if not discarding:
                            self._emit(buffer.rstrip(b"\r").decode("utf-8", errors="replace"))
                        buffer.clear()
                        discarding = False
                    elif not discarding:
                        buffer.append(byte)
                        if len(buffer) > MAX_LINE_BYTES:
                            self._emit("Oversized serial line discarded", diagnostic=True)
                            buffer.clear()
                            discarding = True
                    if self._stop.is_set():
                        break
        except (serial.SerialException, OSError, ValueError) as exc:
            self.error = f"{self.port}: {exc}"
        finally:
            self.connected = False
            if connection is not None:
                try:
                    connection.close()
                except (serial.SerialException, OSError) as exc:
                    self.error = self.error or f"Serial close failed: {exc}"
