import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QCoreApplication, QEvent
from PySide6.QtWidgets import QApplication
import pytest

from link_monitor.models import LogRecord
from link_monitor.storage import SessionWriter
from link_monitor.ui import MonitorWindow

from test_session import CTRL, RX, connected_session, send


@pytest.fixture(scope="module")
def app():
    instance = QApplication.instance() or QApplication([])
    yield instance
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)


@pytest.fixture
def window(app, monkeypatch):
    monkeypatch.setattr("link_monitor.ui.available_ports", lambda: [])
    widget = MonitorWindow()
    widget.show()
    app.processEvents()
    yield widget
    widget.close()
    widget.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)


def test_capture_controls_and_aggregate_nodes(app, monkeypatch):
    monkeypatch.setattr("link_monitor.ui.available_ports", lambda: [])
    session, clock = connected_session()
    widget = MonitorWindow(session)
    try:
        widget.update_controls()
        assert widget.start_button.isEnabled()
        assert not widget.stop_button.isEnabled()
        from pathlib import Path
        session.start(Path("unused.jsonl"))
        widget.update_controls()
        assert widget.stop_button.isEnabled()
        assert not widget.connect_buttons["CTRL"].isEnabled()
        clock[0] += 1
        send(session, clock, 2000, 20, 20)
        clock[0] += 1
        send(session, clock, 3000, 30, 29)
        widget.render_snapshot(session.snapshot(), 2)
        assert widget.nodes[0][0].text().endswith("\n10")
        assert "90.0%" in widget.summary.text()
        session.stop()
        widget.update_controls()
        assert not widget.stop_button.isEnabled()
        assert all(worker.connected for worker in session.workers.values())
    finally:
        widget.close()
        widget.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)


def test_replay_uses_no_serial_and_restarts(window, tmp_path):
    path = tmp_path / "capture.jsonl"
    writer = SessionWriter(path, {})
    for elapsed, count, acks in [(1, 10, 10), (2, 20, 18)]:
        for role, raw in [("CTRL", CTRL.format(t=elapsed * 1000, c=count, a=acks)),
                          ("RX", RX.format(t=elapsed * 1000, c=count))]:
            from link_monitor.parser import parse_line
            writer.write(parse_line(role, raw, elapsed, "now"))
    writer.close({"incomplete": False})
    window.load_replay(path)
    assert not window.session.workers
    window._replay_elapsed = 10
    window.advance_replay()
    assert window._replay_engine.snapshot()["roles"]["CTRL"]["ack_coverage"] == 80
    assert not window._replay_running
    window.restart_replay()
    assert window._replay_running
    assert not window._replay_engine.snapshot()["roles"]["CTRL"]["ready"]
    window.toggle_play()
    assert not window._replay_running
    window.exit_replay()
    assert window._replay_path is None


def test_rate_chart_updates_when_counts_stop_changing(window):
    session, clock = connected_session()
    from pathlib import Path
    session.start(Path("unused.jsonl"))
    clock[0] += 1
    send(session, clock, 2000, 20, 19)
    clock[0] += 1
    send(session, clock, 3000, 30, 28)
    window.render_snapshot(session.snapshot(), 2)
    assert window._history[-1][3] == 10
    clock[0] += 1
    send(session, clock, 4000, 30, 28)
    window.render_snapshot(session.snapshot(), 3)
    assert window._history[-1][3] == 0
    assert len(window._history) == 2
    window.render_snapshot(session.snapshot(), 3)
    assert len(window._history) == 2
    session.close()


def test_bad_replay_has_visible_warning_and_gui_history_is_bounded(window, tmp_path):
    path = tmp_path / "bad.jsonl"
    path.write_text('{"version": 999, "type": "metadata"}\n', encoding="utf-8")
    window.load_replay(path)
    assert window._replay_next is None
    assert "incomplete" in window._replay_warning[0].lower()
    assert not window.session.workers
    snapshot = window._replay_engine.snapshot()
    for index in range(1300):
        snapshot["roles"]["CTRL"]["ready"] = True
        snapshot["roles"]["CTRL"]["totals"]["controls"] = index
        window.render_snapshot(snapshot, index + 1)
    assert len(window._history) == 1200
