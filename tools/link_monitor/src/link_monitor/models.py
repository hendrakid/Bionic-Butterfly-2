"""Transport-independent observations from the firmware serial log."""

from dataclasses import dataclass, field


@dataclass
class LogRecord:
    role: str
    raw: str
    elapsed: float
    wall_time: str
    fields: dict[str, int | str] = field(default_factory=dict)
    kind: str = "raw"

    def to_dict(self) -> dict:
        return {
            "role": self.role, "raw": self.raw, "elapsed": self.elapsed,
            "wall_time": self.wall_time, "fields": self.fields, "kind": self.kind,
        }

    @classmethod
    def from_dict(cls, value: dict) -> "LogRecord":
        if not isinstance(value, dict) or not isinstance(value.get("fields", {}), dict):
            raise ValueError("Invalid log record")
        return cls(
            role=value["role"], raw=value["raw"], elapsed=value["elapsed"],
            wall_time=value["wall_time"], fields=value.get("fields", {}),
            kind=value.get("kind", "raw"),
        )
