import csv
import json
import os
import pytest

from link_monitor.metrics import MetricsEngine
from link_monitor.models import LogRecord
from link_monitor.parser import parse_line
from link_monitor.replay import ReplayError, iter_records, iter_rows
from link_monitor.storage import SessionWriter, export_csv


CTRL = "CTRL t={t} controls={controls} confirmed_acks={acks} ack_rtt_ms=3 confirmed=1 radio_ok=10 radio_fail=0 rx_overflow=0 rx_high=2 ack_rejected=1 send_fail=0 missed_slots=0"


def record(t, controls, acks, elapsed):
    return parse_line("CTRL", CTRL.format(t=t, controls=controls, acks=acks), elapsed, "wall")


def test_roundtrip_metadata_lifecycle_and_live_replay_metrics(tmp_path):
    path = tmp_path / "session.jsonl"
    records = [record(1000, 10, 9, 1), record(2000, 20, 17, 2), parse_line("CTRL", "Bionic Butterfly Controller", 3, ""), record(1000, 10, 8, 4), record(2000, 20, 16, 5)]
    engine = MetricsEngine()
    expected = []
    writer = SessionWriter(path, {"ports": {"CTRL": "COM4"}, "capture_id": "test"})
    writer.event("start", 0, {})
    for item in records:
        writer.write(item)
        expected.append(engine.ingest(item))
    writer.close(engine.snapshot())
    rows = list(iter_rows(path))
    assert rows[0]["metadata"]["ports"]["CTRL"] == "COM4"
    assert [r["index"] for r in rows] == list(range(len(rows)))
    assert rows[-1]["summary"] == engine.snapshot()
    replay = MetricsEngine()
    actual = []
    observations = list(iter_records(path))
    assert json.loads(observations[0].fields["details"])["capture_id"] == "test"
    assert observations[1].fields["event"] == "start"
    for item in observations:
        result = replay.ingest(item)
        if item.role == "CTRL":
            actual.append(result)
    assert actual == expected
    assert [r for r in observations if r.role == "CTRL"] == records


def test_lifecycle_discontinuity_and_incomplete_translate_for_replay(tmp_path):
    path = tmp_path / "events.jsonl"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.event("discontinuity", 2, {"role": "CTRL", "reason": "USB boot"})
    writer.event("incomplete", 3, {"reason": "disk error"})
    writer.close()
    engine = MetricsEngine()
    for item in iter_records(path):
        result = engine.ingest(item)
    assert result["roles"]["CTRL"]["segment"] == 1
    assert "capture incomplete" in result["quality"]


def test_csv_is_replay_derived_and_na_not_zero(tmp_path):
    path, output = tmp_path / "session.jsonl", tmp_path / "metrics.csv"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.write(record(2000, 20, 17, 2))
    writer.close()
    export_csv(path, output)
    with output.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    coverage = [r for r in rows if r["metric"] == "ack_coverage"]
    assert [r["value"] for r in coverage] == ["", "80.0"]
    assert all(r["unit"] == "%" for r in coverage)
    rate = [r for r in rows if r["category"] == "rates" and r["metric"] == "controls"]
    assert rate[-1]["value"] == "10.0" and rate[-1]["unit"] == "count/s"
    assert "waiting" in coverage[0]["quality"]
    with pytest.raises(ValueError, match="overwrite"):
        export_csv(path, path)


@pytest.mark.parametrize("alias", ["identical", "normalized", "case_variant"])
def test_export_rejects_source_alias_without_changing_capture(tmp_path, alias):
    path = tmp_path / "capture.jsonl"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.close({"quality": ["preserve"]})
    before = path.read_bytes()
    destination = {
        "identical": path,
        "normalized": path.parent / "." / path.name,
        "case_variant": path.with_name(path.name.upper()),
    }[alias]
    with pytest.raises(ValueError, match="overwrite"):
        export_csv(path, destination)
    assert path.read_bytes() == before


def test_export_rejects_hardlink_without_changing_capture(tmp_path):
    path, alias = tmp_path / "capture.jsonl", tmp_path / "alias.csv"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.close()
    try:
        os.link(path, alias)
    except OSError as exc:
        pytest.skip(f"Hardlinks unavailable: {exc}")
    before = path.read_bytes()
    with pytest.raises(ValueError, match="overwrite"):
        export_csv(path, alias)
    assert path.read_bytes() == before == alias.read_bytes()


def test_truncated_final_line_warns_and_preserves_complete_records(tmp_path):
    path = tmp_path / "partial.jsonl"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.close()
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    path.write_text("".join(lines[:-1]) + '{"version":1,"type":"summary"', encoding="utf-8")
    with pytest.warns(RuntimeWarning) as caught:
        records = list(iter_records(path))
    assert any("Truncated final" in str(w.message) for w in caught)
    assert any("missing final summary" in str(w.message) for w in caught)
    assert len(records) == 3 and records[1].fields["controls"] == 10
    assert records[-1].fields["event"] == "incomplete"


@pytest.mark.parametrize("contents", [
    '{"version":2,"type":"metadata","metadata":{}}\n',
    '{"version":1,"type":"metadata","metadata":{}}\nnot json\n',
    '{"version":1,"type":"metadata","metadata":{}}\n{"version":1,"type":"record","record":{}}\n',
    '{"version":1,"type":"metadata","metadata":{}}\n{"version":1,"type":"event","kind":"boot","elapsed":-1,"details":{}}\n',
    '',
])
def test_unsupported_or_corrupt_capture_rejected(tmp_path, contents):
    path = tmp_path / "bad.jsonl"
    path.write_text(contents, encoding="utf-8")
    with pytest.raises(ReplayError):
        list(iter_records(path))


def test_disk_write_error_propagates_and_close_still_closes(tmp_path):
    path = tmp_path / "disk.jsonl"
    writer = SessionWriter(path, {})
    real_stream = writer._stream

    class FailedDisk:
        def write(self, payload):
            raise OSError("disk full")

        def flush(self):
            real_stream.flush()

        def close(self):
            real_stream.close()

    writer._stream = FailedDisk()
    with pytest.raises(OSError, match="disk full"):
        writer.write(record(1000, 10, 9, 1))
    with pytest.raises(OSError, match="disk full"):
        writer.close({"incomplete": True})
    assert real_stream.closed
    with pytest.warns(RuntimeWarning, match="missing final summary"):
        assert len(list(iter_rows(path))) == 1

def test_storage_failures_propagate_and_partial_file_survives(tmp_path):
    path = tmp_path / "session.jsonl"
    writer = SessionWriter(path, {})
    writer.write(LogRecord("CTRL", "raw", 1, ""))
    with pytest.raises(ValueError):
        writer.event("invalid", float("nan"), {})
    writer.close()
    writer.close()
    with pytest.raises(ValueError, match="closed"):
        writer.write(LogRecord("CTRL", "raw", 2, ""))
    with pytest.raises(FileExistsError):
        SessionWriter(path, {})
    rows = list(iter_rows(path))
    assert len(rows) == 3 and rows[-1]["summary"] == {}
    with pytest.raises(OSError):
        SessionWriter(tmp_path / "missing" / "session.jsonl", {})


def test_incomplete_summary_quality_and_storage_error_survive_replay_and_csv(tmp_path):
    path, csv_path = tmp_path / "incomplete.jsonl", tmp_path / "incomplete.csv"
    writer = SessionWriter(path, {})
    live = MetricsEngine()
    for item in [record(1000, 10, 9, 1), record(2000, 20, 17, 2)]:
        writer.write(item)
        live.ingest(item)
    summary = {
        "incomplete": True,
        "quality": ["RX: USB disconnected", "storage backlog overflow"],
        "storage_error": "Storage backlog overflow: capture incomplete",
    }
    writer.close(summary)
    expected = live.snapshot()
    expected["quality"] = sorted(set(expected["quality"] + summary["quality"] + [
        "capture incomplete", f"capture storage error: {summary['storage_error']}",
    ]))
    replay = MetricsEngine()
    for item in iter_records(path):
        actual = replay.ingest(item)
    assert actual == expected
    export_csv(path, csv_path)
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    summary_row = [row for row in rows if row["role"] == "SESSION" and row["value"] == "summary"]
    assert len(summary_row) == 1
    assert summary_row[0]["unit"] == "status"
    assert "USB disconnected" in summary_row[0]["quality"]
    assert "storage error" in summary_row[0]["quality"]


def test_live_quality_event_record_roundtrip_matches_replay(tmp_path):
    path, csv_path = tmp_path / "lifecycle.jsonl", tmp_path / "lifecycle.csv"
    writer = SessionWriter(path, {})
    live = MetricsEngine()
    stats = record(1000, 10, 9, 1)
    lifecycle = LogRecord("SESSION", "", 2, "", {
        "event": "disconnect", "details": json.dumps({
            "incomplete": True, "quality": ["RX unplugged"],
        }),
    }, "event")
    for item in [stats, lifecycle]:
        writer.write(item)
        live.ingest(item)
    writer.close(live.snapshot())
    replay = MetricsEngine()
    for item in iter_records(path):
        replay.ingest(item)
    assert replay.snapshot() == live.snapshot()
    export_csv(path, csv_path)
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert any(row["role"] == "SESSION" and row["value"] == "disconnect" and
               "RX unplugged" in row["quality"] for row in rows)


def test_missing_final_summary_warns_and_marks_replay_incomplete(tmp_path):
    path, csv_path = tmp_path / "crashed.jsonl", tmp_path / "crashed.csv"
    writer = SessionWriter(path, {})
    writer.write(record(1000, 10, 9, 1))
    writer.close()
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    path.write_text("".join(lines[:-1]), encoding="utf-8")
    with pytest.warns(RuntimeWarning, match="missing final summary"):
        observations = list(iter_records(path))
    assert observations[-1].fields["event"] == "incomplete"
    engine = MetricsEngine()
    for item in observations:
        snapshot = engine.ingest(item)
    assert "capture incomplete" in snapshot["quality"]
    with pytest.warns(RuntimeWarning, match="missing final summary"):
        export_csv(path, csv_path)
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert any(row["role"] == "SESSION" and row["value"] == "incomplete" and
               "capture incomplete" in row["quality"] for row in rows)
