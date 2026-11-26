"""Streaming, validated reading of versioned capture files."""

import json
import math
from pathlib import Path
import warnings
from collections.abc import Iterator

from .models import LogRecord

SCHEMA_VERSION = 1


class ReplayError(ValueError):
    pass


def iter_rows(path: Path) -> Iterator[dict]:
    """Only an unfinished final JSON line is recoverable; interior corruption is fatal."""
    with Path(path).open("r", encoding="utf-8") as stream:
        seen_metadata = False
        seen_summary = False
        for number, line in enumerate(stream, 1):
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                if not line.endswith("\n") and not stream.read(1):
                    warnings.warn(f"Truncated final JSONL line {number} ignored", RuntimeWarning, stacklevel=2)
                    break
                raise ReplayError(f"Malformed JSONL line {number}") from exc
            if not isinstance(row, dict) or row.get("version") != SCHEMA_VERSION:
                raise ReplayError(f"Unsupported or missing schema version at line {number}")
            kind = row.get("type")
            if kind not in {"metadata", "record", "event", "summary"}:
                raise ReplayError(f"Unknown row type at line {number}")
            if number == 1 and kind != "metadata":
                raise ReplayError("Capture must begin with metadata")
            if seen_summary:
                raise ReplayError(f"Data after final summary at line {number}")
            if kind == "metadata":
                if seen_metadata or not isinstance(row.get("metadata"), dict):
                    raise ReplayError(f"Invalid metadata at line {number}")
                seen_metadata = True
            elif kind == "record":
                try:
                    record = LogRecord.from_dict(row["record"])
                    if record.role not in {"CTRL", "RX", "SESSION"} or record.kind not in {"stats", "event", "boot", "raw", "malformed", "mismatch"} or not isinstance(record.raw, str) or not isinstance(record.wall_time, str) or type(record.elapsed) not in {int, float} or not math.isfinite(record.elapsed) or record.elapsed < 0 or any(not isinstance(key, str) or type(value) not in {int, str} for key, value in record.fields.items()):
                        raise ValueError("Invalid record fields")
                except (KeyError, TypeError, ValueError) as exc:
                    raise ReplayError(f"Invalid record at line {number}") from exc
            elif kind == "event":
                if not isinstance(row.get("kind"), str) or not isinstance(row.get("details"), dict) or type(row.get("elapsed")) not in {int, float} or not math.isfinite(row["elapsed"]) or row["elapsed"] < 0:
                    raise ReplayError(f"Invalid event at line {number}")
            elif not isinstance(row.get("summary"), dict):
                raise ReplayError(f"Invalid summary at line {number}")
            elif kind == "summary":
                seen_summary = True
            yield row
        if not seen_metadata:
            raise ReplayError("Capture has no metadata")
        if not seen_summary:
            warnings.warn("Capture missing final summary; incomplete", RuntimeWarning, stacklevel=2)


def iter_records(path: Path) -> Iterator[LogRecord]:
    """Lifecycle and metadata are event observations, preserving their original details."""
    elapsed = 0.0
    seen_summary = False
    for row in iter_rows(path):
        if row["type"] == "record":
            record = LogRecord.from_dict(row["record"])
            elapsed = record.elapsed
            yield record
        elif row["type"] == "event":
            details = row["details"]
            fields = {key: value for key, value in details.items() if isinstance(key, str) and type(value) in {int, str}}
            fields.update({"event": row["kind"], "details": json.dumps(details, ensure_ascii=False, sort_keys=True)})
            elapsed = row["elapsed"]
            yield LogRecord(details.get("role", "SESSION"), "", elapsed, "", fields, "event")
        else:
            kind = row["type"]
            if kind == "summary":
                seen_summary = True
            yield LogRecord("SESSION", "", elapsed, "", {"event": kind, "details": json.dumps(row[kind], ensure_ascii=False, sort_keys=True)}, "event")
    if not seen_summary:
        yield LogRecord("SESSION", "", elapsed, "", {"event": "incomplete", "details": json.dumps({"reason": "missing final summary"})}, "event")
