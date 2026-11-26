"""Append-only streaming capture and replay-derived aggregate CSV export."""

import csv
import json
from pathlib import Path

from .metrics import MetricsEngine
from .models import LogRecord
from .replay import SCHEMA_VERSION, iter_records


class SessionWriter:
    def __init__(self, path: Path, metadata: dict) -> None:
        self.path = Path(path)
        self._stream = self.path.open("x", encoding="utf-8", newline="\n")
        self._index = 0
        self._closed = False
        try:
            self._append({"type": "metadata", "metadata": metadata})
        except Exception:
            self._stream.close()
            self._closed = True
            raise

    def _append(self, row: dict) -> None:
        if self._closed:
            raise ValueError("Capture writer is closed")
        row.update({"version": SCHEMA_VERSION, "index": self._index})
        payload = json.dumps(row, ensure_ascii=False, allow_nan=False)
        self._stream.write(payload + "\n")
        self._stream.flush()
        self._index += 1

    def write(self, record: LogRecord) -> None:
        self._append({"type": "record", "record": record.to_dict()})

    def event(self, kind: str, elapsed: float, details: dict) -> None:
        self._append({"type": "event", "kind": kind, "elapsed": elapsed, "details": details})

    def close(self, summary: dict | None = None) -> None:
        if self._closed:
            return
        try:
            self._append({"type": "summary", "summary": summary if summary is not None else {}})
            self._stream.flush()
        finally:
            self._closed = True
            self._stream.close()


def export_csv(jsonl_path: Path, csv_path: Path) -> None:
    """One long-format row per derived metric, never one row per radio packet."""
    source, destination = Path(jsonl_path), Path(csv_path)
    if source.resolve() == destination.resolve() or (
        source.exists() and destination.exists() and source.samefile(destination)
    ):
        raise ValueError("CSV output must not overwrite the capture")
    engine = MetricsEngine()
    with Path(csv_path).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["elapsed", "wall_time", "role", "segment", "ready", "category", "metric", "value", "unit", "quality"])
        for record in iter_records(jsonl_path):
            snapshot = engine.ingest(record)
            if record.role not in snapshot["roles"]:
                if record.role == "SESSION" and record.kind == "event" and record.fields.get("event") != "metadata":
                    writer.writerow([record.elapsed, record.wall_time, "SESSION", "", "", "quality", "event", record.fields.get("event", ""), "status", "; ".join(snapshot["quality"])])
                continue
            state = snapshot["roles"][record.role]
            groups = {key: state[key] for key in ("totals", "rates", "gauges", "status")}
            groups["derived"] = {"ack_coverage": state["ack_coverage"], "unconfirmed": state["unconfirmed"]}
            for category, values in groups.items():
                for metric, value in values.items():
                    if category == "rates":
                        unit = "count/s"
                    elif metric == "ack_coverage":
                        unit = "%"
                    elif metric.endswith("_ms") or metric == "t":
                        unit = "ms"
                    elif metric == "elapsed":
                        unit = "s"
                    elif category == "status":
                        unit = "status"
                    else:
                        unit = "count"
                    writer.writerow([record.elapsed, record.wall_time, record.role, state["segment"], state["ready"], category, metric, "" if value is None else value, unit, "; ".join(snapshot["quality"])])
