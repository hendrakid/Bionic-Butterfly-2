"""Segmented aggregate metrics; MAC callbacks are never application delivery."""

from copy import deepcopy
import json
import math

from .models import LogRecord
from .parser import _REQUIRED

_COUNTERS = {
    "CTRL": tuple("controls confirmed_acks radio_ok radio_fail rx_overflow ack_rejected send_fail missed_slots".split()),
    "RX": tuple("controls rejected expired acks ack_fail radio_ok radio_fail rx_overflow timeouts rx_frames rx_invalid rx_oversized future".split()),
}
_GAUGES = {"CTRL": ("ack_rtt_ms", "rx_high"), "RX": ("rx_high",)}
_STATUS = {"CTRL": ("confirmed",), "RX": ("timed_out",)}
_UINT32 = 1 << 32
_WRAP_WINDOW = 60_000
_LOG_STALE_SECONDS = 3.0


def _delta(previous: int, current: int) -> int | None:
    if current >= previous:
        return current - previous
    if previous >= _UINT32 - _WRAP_WINDOW and current <= _WRAP_WINDOW:
        return _UINT32 - previous + current
    return None


def _empty(role: str, segment: int = 0) -> dict:
    return {
        "totals": {key: None for key in _COUNTERS[role]},
        "rates": {key: None for key in _COUNTERS[role]},
        "gauges": {key: None for key in _GAUGES[role]},
        "status": {}, "segment": segment, "ready": False,
        "ack_coverage": None, "unconfirmed": None,
    }


class MetricsEngine:
    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self._roles = {role: _empty(role) for role in _COUNTERS}
        self._previous: dict[str, LogRecord] = {}
        self._quality: set[str] = set()
        self._elapsed = 0.0

    def _restart(self, role: str, reason: str) -> None:
        self._roles[role] = _empty(role, self._roles[role]["segment"] + 1)
        self._previous.pop(role, None)
        self._quality.add(f"{role}: {reason}; rebaseline")
        self._quality.add("crossboard comparison unreliable after discontinuity")

    def _session_quality(self, record: LogRecord) -> None:
        details = record.fields
        if "details" in record.fields:
            try:
                details = json.loads(record.fields["details"])
            except (TypeError, ValueError):
                self._quality.add("capture malformed lifecycle details")
                return
        if not isinstance(details, dict):
            self._quality.add("capture malformed lifecycle details")
            return
        flags = details.get("quality", [])
        if isinstance(flags, str):
            flags = [flags]
        if isinstance(flags, list):
            self._quality.update(flag for flag in flags if isinstance(flag, str) and flag)
        if details.get("incomplete"):
            self._quality.add("capture incomplete")
        storage_error = details.get("storage_error")
        if storage_error:
            self._quality.add("capture incomplete")
            self._quality.add(f"capture storage error: {storage_error}")

    def ingest(self, record: LogRecord) -> dict:
        if not isinstance(record.elapsed, (int, float)) or not math.isfinite(record.elapsed) or record.elapsed < 0:
            self._quality.add("invalid PC elapsed timestamp")
            return self.snapshot()
        self._elapsed = max(self._elapsed, record.elapsed)
        role = record.role
        if record.kind == "event":
            event = record.fields.get("event", "")
            if event in {"incomplete", "overflow", "error", "disconnect"}:
                self._quality.add(f"capture {event}")
            if role == "SESSION" and event != "metadata":
                self._session_quality(record)
        if role not in self._roles:
            return self.snapshot()
        state = self._roles[role]
        if record.kind == "boot" or (record.kind == "event" and record.fields.get("event") in {"boot", "discontinuity"}):
            self._restart(role, "boot/discontinuity")
        elif record.kind in {"malformed", "mismatch"}:
            self._quality.add(f"{role}: {record.kind} data; metrics unavailable")
            state["ready"] = False
            state["status"]["data_valid"] = False
            state["rates"] = {key: None for key in _COUNTERS[role]}
            state["ack_coverage"] = state["unconfirmed"] = None
            state["gauges"] = {key: None for key in _GAUGES[role]}
        elif record.kind == "event":
            state["status"].update(record.fields)
            if "link" in record.fields:
                linked = record.fields["link"] != "TIMEOUT"
                state["status"]["confirmed" if role == "CTRL" else "timed_out"] = int(linked if role == "CTRL" else not linked)
            if role == "CTRL" and "rtt_ms" in record.fields:
                state["gauges"]["ack_rtt_ms"] = record.fields["rtt_ms"]
        elif record.kind == "stats":
            self._stats(record)
        return self.snapshot()

    def _stats(self, record: LogRecord) -> None:
        role, fields = record.role, record.fields
        if not _REQUIRED[role].issubset(fields) or any(type(fields[key]) is not int or not 0 <= fields[key] < _UINT32 for key in _REQUIRED[role]) or any(fields[key] not in (0, 1) for key in _STATUS[role]):
            self.ingest(LogRecord(role, record.raw, record.elapsed, record.wall_time, fields, "malformed"))
            return
        previous = self._previous.get(role)
        deltas = None
        seconds = None
        if previous is not None:
            milliseconds = _delta(previous.fields["t"], fields["t"])
            pc_seconds = record.elapsed - previous.elapsed
            deltas = {key: _delta(previous.fields[key], fields[key]) for key in _COUNTERS[role]}
            wrapped = any(fields[key] < previous.fields[key] for key in _COUNTERS[role])
            # A narrow boundary alone is insufficient: reject implausible wrap jumps.
            wrap_plausible = all(value is not None and (not wrapped or value <= max(1.0, pc_seconds) * 10_000) for value in deltas.values())
            if milliseconds is None or milliseconds <= 0 or pc_seconds <= 0 or abs(milliseconds / 1000 - pc_seconds) > max(5.0, pc_seconds * 0.5) or not wrap_plausible:
                self._restart(role, "counter/time discontinuity")
                previous = None
            else:
                seconds = milliseconds / 1000
        state = self._roles[role]
        state["gauges"] = {key: fields[key] for key in _GAUGES[role]}
        state["status"].update({key: fields[key] for key in _STATUS[role]})
        state["status"].update({"t": fields["t"], "elapsed": record.elapsed, "log_stale": False, "data_valid": True})
        if previous is None:
            state["totals"] = {key: 0 for key in _COUNTERS[role]}
            state["rates"] = {key: None for key in _COUNTERS[role]}
            state["ready"] = False
        else:
            for key, value in deltas.items():
                state["totals"][key] += value
                state["rates"][key] = value / seconds
            state["ready"] = True
            if role == "CTRL":
                controls, acks = state["totals"]["controls"], state["totals"]["confirmed_acks"]
                state["ack_coverage"] = 100.0 * acks / controls if controls else None
                state["unconfirmed"] = controls - acks if acks <= controls else None
                if acks > controls:
                    self._quality.add("CTRL: ACK boundary/data anomaly (confirmed exceeds submitted)")
        self._previous[role] = record

    def snapshot(self) -> dict:
        roles = deepcopy(self._roles)
        quality = set(self._quality)
        for role, state in roles.items():
            if state["status"].get("data_valid") is False:
                state["totals"] = {key: None for key in _COUNTERS[role]}
                for key in _STATUS[role]:
                    state["status"][key] = None
            if role not in self._previous:
                quality.add(f"{role}: waiting for baseline")
            elif not state["ready"]:
                quality.add(f"{role}: waiting for valid interval")
            previous = self._previous.get(role)
            if previous and self._elapsed - previous.elapsed > _LOG_STALE_SECONDS:
                quality.add(f"{role}: serial log stale (not radio link loss)")
                state["status"]["log_stale"] = True
                state["rates"] = {key: None for key in _COUNTERS[role]}
        return {"roles": roles, "quality": sorted(quality)}
