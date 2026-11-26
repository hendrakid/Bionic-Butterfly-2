"""Parse existing controller/receiver diagnostic lines without inferring radio packets."""

import re

from .models import LogRecord


_REQUIRED = {
    "CTRL": frozenset("t controls confirmed_acks ack_rtt_ms confirmed radio_ok radio_fail rx_overflow rx_high ack_rejected send_fail missed_slots".split()),
    "RX": frozenset("t controls rejected expired acks ack_fail timed_out radio_ok radio_fail rx_overflow rx_high timeouts rx_frames rx_invalid rx_oversized future".split()),
}
_NUMERIC = _REQUIRED["CTRL"] | _REQUIRED["RX"] | frozenset("seq rtt_ms recovery_ms age_ms channel".split())
_STAT_HINTS = frozenset({"t", "controls", "confirmed_acks", "acks", "rx_frames"})
_PAIR = re.compile(r"^([a-z][a-z0-9_]*)=(\S+)$")
_ROLE = re.compile(r"^(CTRL|RX)(?:\s|$)")
_BANNER = {"Bionic Butterfly Controller": "CTRL", "Bionic Butterfly Receiver": "RX"}
_MAX_LINE = 4096


def parse_line(role: str, raw: str, elapsed: float, wall_time: str) -> LogRecord:
    """Keep the original line even when an observation cannot be trusted."""
    record = LogRecord(role, raw, elapsed, wall_time)
    if len(raw) > _MAX_LINE or "\x00" in raw:
        record.kind = "malformed"
        return record
    text = raw.strip()
    if text in _BANNER:
        record.kind = "boot" if _BANNER[text] == role else "mismatch"
        return record
    match = _ROLE.match(text)
    if not match:
        return record
    if match.group(1) != role:
        record.kind = "mismatch"
        return record
    tokens = text.split()[1:]
    fields: dict[str, int | str] = {}
    invalid = False
    for token in tokens:
        pair = _PAIR.fullmatch(token)
        if pair is None:
            invalid = True
            continue
        key, value = pair.groups()
        if key in fields:
            invalid = True
            continue
        if key in _NUMERIC:
            if not value.isascii() or not value.isdecimal():
                invalid = True
                continue
            number = int(value)
            if number > 0xFFFFFFFF or (key in {"confirmed", "timed_out"} and number not in (0, 1)):
                invalid = True
                continue
            fields[key] = number
        else:
            fields[key] = value
    record.fields = fields
    if "transport" in fields:
        record.kind = "malformed" if invalid else "event"
    elif "link" in fields:
        record.kind = "malformed" if invalid or fields["link"] not in {"confirmed", "recovered", "TIMEOUT"} else "event"
    elif _STAT_HINTS.intersection(fields) or any(token.split("=", 1)[0] in _STAT_HINTS for token in tokens):
        record.kind = "stats" if not invalid and _REQUIRED[role].issubset(fields) else "malformed"
    else:
        record.kind = "raw"
    return record
