import pytest

from link_monitor.models import LogRecord
from link_monitor.parser import parse_line

CTRL = "CTRL t=1000 controls=10 confirmed_acks=9 ack_rtt_ms=3 confirmed=1 radio_ok=10 radio_fail=0 rx_overflow=0 rx_high=2 ack_rejected=1 send_fail=0 missed_slots=0"
RX = "RX t=1000 controls=9 rejected=1 expired=2 acks=9 ack_fail=0 timed_out=0 radio_ok=9 radio_fail=0 rx_overflow=0 rx_high=2 timeouts=0 rx_frames=12 rx_invalid=0 rx_oversized=0 future=1"


@pytest.mark.parametrize("role,line", [("CTRL", CTRL), ("RX", RX)])
def test_real_stats_and_extra_fields(role, line):
    record = parse_line(role, line + " build=next", 1.25, "wall")
    assert record.kind == "stats"
    assert record.fields["controls"] == (10 if role == "CTRL" else 9)
    assert record.fields["build"] == "next"
    assert LogRecord.from_dict(record.to_dict()) == record
    assert record.elapsed == 1.25 and record.wall_time == "wall"


@pytest.mark.parametrize("line", [CTRL.replace(" controls=10", ""), CTRL.replace("controls=10", "controls=no"), CTRL + " controls=12", CTRL.replace("confirmed=1", "confirmed=2"), CTRL.replace("t=1000", "t=4294967296"), "CTRL t=", "CTRL " + "a" * 4096])
def test_invalid_stats_preserve_raw(line):
    record = parse_line("CTRL", line, 0, "")
    assert record.kind == "malformed"
    assert record.raw == line


@pytest.mark.parametrize("role,line,kind", [
    ("CTRL", "Bionic Butterfly Controller", "boot"),
    ("RX", "Bionic Butterfly Receiver", "boot"),
    ("CTRL", RX, "mismatch"),
    ("RX", "Bionic Butterfly Controller", "mismatch"),
    ("CTRL", "CTRL link=confirmed seq=2 rtt_ms=4", "event"),
    ("RX", "RX link=TIMEOUT safe_state=DISARMED servo=disabled age_ms=500", "event"),
    ("CTRL", "CTRL transport=READY peer=AA:BB channel=6", "event"),
    ("RX", "unrecognized boot ROM message", "raw"),
    ("CTRL", "CTRL diagnostic not-a-stat", "raw"),
])
def test_banner_events_and_fallback(role, line, kind):
    assert parse_line(role, line, 0, "").kind == kind


def test_raw_line_terminators_and_mutable_diagnostic_kind():
    record = parse_line("CTRL", CTRL + "\r\n", 1, "")
    assert record.kind == "stats" and record.raw.endswith("\r\n")
    record.kind = "malformed"
    assert record.to_dict()["kind"] == "malformed"
