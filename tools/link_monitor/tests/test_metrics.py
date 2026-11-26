import pytest

from link_monitor.metrics import MetricsEngine
from link_monitor.models import LogRecord
from link_monitor.parser import parse_line


BASE = {
    "CTRL": dict(t=1000, controls=10, confirmed_acks=9, ack_rtt_ms=3, confirmed=1, radio_ok=10, radio_fail=0, rx_overflow=0, rx_high=2, ack_rejected=1, send_fail=0, missed_slots=0),
    "RX": dict(t=1000, controls=9, rejected=1, expired=2, acks=9, ack_fail=0, timed_out=0, radio_ok=9, radio_fail=0, rx_overflow=0, rx_high=2, timeouts=0, rx_frames=12, rx_invalid=0, rx_oversized=0, future=1),
}


def stats(role="CTRL", elapsed=1, **changes):
    fields = BASE[role] | changes
    return parse_line(role, role + " " + " ".join(f"{k}={v}" for k, v in fields.items()), elapsed, "wall")


def test_baseline_deltas_rates_and_last_sample_gauge():
    engine = MetricsEngine()
    baseline = engine.ingest(stats())["roles"]["CTRL"]
    assert not baseline["ready"] and baseline["ack_coverage"] is None
    result = engine.ingest(stats(elapsed=2, t=2000, controls=20, confirmed_acks=17, ack_rtt_ms=1, rx_high=1))["roles"]["CTRL"]
    assert result["totals"]["controls"] == 10
    assert result["rates"]["controls"] == 10
    assert result["ack_coverage"] == 80
    assert result["unconfirmed"] == 2
    assert result["gauges"] == {"ack_rtt_ms": 1, "rx_high": 1}
    result["totals"]["controls"] = 999
    assert engine.snapshot()["roles"]["CTRL"]["totals"]["controls"] == 10


def test_zero_denominator_and_ack_boundary_are_not_clamped():
    engine = MetricsEngine()
    engine.ingest(stats())
    assert engine.ingest(stats(elapsed=2, t=2000))["roles"]["CTRL"]["ack_coverage"] is None
    result = engine.ingest(stats(elapsed=3, t=3000, controls=11, confirmed_acks=11))
    assert result["roles"]["CTRL"]["ack_coverage"] == 200
    assert result["roles"]["CTRL"]["unconfirmed"] is None
    assert any("boundary" in q for q in result["quality"])


@pytest.mark.parametrize("changes", [{"controls": 1}, {"t": 10}, {"t": 1000}, {"t": 1000000}])
def test_discontinuity_rebaselines_and_crossboard_is_unreliable(changes):
    engine = MetricsEngine()
    engine.ingest(stats())
    engine.ingest(stats("RX"))
    result = engine.ingest(stats(elapsed=2, **({"t": 2000} | changes)))
    assert result["roles"]["CTRL"]["segment"] == 1
    assert not result["roles"]["CTRL"]["ready"]
    assert result["roles"]["RX"]["segment"] == 0
    assert any("crossboard" in q for q in result["quality"])


def test_legitimate_uint32_counter_and_uptime_wrap():
    engine = MetricsEngine()
    engine.ingest(stats(t=4294967000, controls=4294967290, confirmed_acks=4294967288))
    result = engine.ingest(stats(elapsed=2, t=704, controls=4, confirmed_acks=0))["roles"]["CTRL"]
    assert result["segment"] == 0
    assert result["totals"]["controls"] == 10
    assert result["totals"]["confirmed_acks"] == 8
    assert result["rates"]["controls"] == 10


def test_ambiguous_wrap_is_reset_not_huge_delta():
    engine = MetricsEngine()
    engine.ingest(stats(controls=4294907296))
    result = engine.ingest(stats(elapsed=2, t=2000, controls=1000))["roles"]["CTRL"]
    assert result["segment"] == 1 and not result["ready"]


def test_boot_malformed_events_stale_and_reset():
    engine = MetricsEngine()
    engine.ingest(stats())
    engine.ingest(stats(elapsed=2, t=2000, controls=20, confirmed_acks=19))
    result = engine.ingest(parse_line("CTRL", "CTRL t=3000 controls=no", 3, ""))
    assert not result["roles"]["CTRL"]["ready"]
    assert result["roles"]["CTRL"]["ack_coverage"] is None
    assert result["roles"]["CTRL"]["gauges"]["ack_rtt_ms"] is None
    assert result["roles"]["CTRL"]["totals"]["controls"] is None
    engine.ingest(parse_line("CTRL", "CTRL link=confirmed seq=4 rtt_ms=7", 3, ""))
    assert engine.snapshot()["roles"]["CTRL"]["gauges"]["ack_rtt_ms"] == 7
    result = engine.ingest(LogRecord("RX", "unrelated", 8, ""))
    assert result["roles"]["CTRL"]["status"]["log_stale"]
    assert any("not radio" in q for q in result["quality"])
    engine.ingest(parse_line("CTRL", "Bionic Butterfly Controller", 9, ""))
    assert engine.snapshot()["roles"]["CTRL"]["segment"] == 1
    engine.reset()
    assert engine.snapshot()["roles"]["CTRL"]["segment"] == 0
    assert not engine.snapshot()["roles"]["CTRL"]["ready"]


def test_recovery_after_malformed_uses_last_valid_counters():
    engine = MetricsEngine()
    engine.ingest(stats())
    engine.ingest(parse_line("CTRL", "CTRL t=2000 controls=no", 2, ""))
    result = engine.ingest(stats(elapsed=3, t=3000, controls=30, confirmed_acks=25))["roles"]["CTRL"]
    assert result["ready"] and result["segment"] == 0
    assert result["totals"]["controls"] == 20
    assert result["rates"]["controls"] == 10
    assert result["ack_coverage"] == 80


def test_receiver_expired_future_subset_and_timeout_status():
    engine = MetricsEngine()
    engine.ingest(stats("RX"))
    result = engine.ingest(stats("RX", elapsed=2, t=2000, expired=5, future=2))["roles"]["RX"]
    assert result["totals"]["expired"] == 3
    assert result["totals"]["future"] == 1
    assert result["ack_coverage"] is None and result["unconfirmed"] is None
    engine.ingest(parse_line("RX", "RX link=TIMEOUT age_ms=500", 2, ""))
    assert engine.snapshot()["roles"]["RX"]["status"]["timed_out"] == 1
