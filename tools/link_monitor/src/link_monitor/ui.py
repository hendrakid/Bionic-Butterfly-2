from __future__ import annotations

from collections import deque
from datetime import datetime
from pathlib import Path
from queue import Empty, Queue
import threading
import time
import warnings

from PySide6.QtCore import QSettings, QTimer
from PySide6.QtWidgets import (
    QComboBox, QDoubleSpinBox, QFileDialog, QGroupBox, QHBoxLayout, QLabel,
    QLineEdit, QMainWindow, QMessageBox, QPlainTextEdit, QPushButton,
    QTableWidget, QTableWidgetItem, QTabWidget, QVBoxLayout, QWidget,
)
import pyqtgraph as pg

# Qt widgets are disposed explicitly before QApplication exits, not during atexit.
pg.setConfigOption("exitCleanup", False)

from .metrics import MetricsEngine
from .replay import iter_records
from .serial_worker import available_ports
from .session import LOG_STALE_SECONDS, MonitorSession
from .storage import export_csv

HISTORY_CAPACITY = 1200


def display(value, digits=1):
    if value is None:
        return "N/A"
    if isinstance(value, float):
        return f"{value:.{digits}f}"
    return str(value)


class MonitorWindow(QMainWindow):
    def __init__(self, session: MonitorSession | None = None):
        super().__init__()
        self.setWindowTitle("Link Monitor — Bionic Butterfly")
        self.resize(1180, 850)
        self.session = session or MonitorSession()
        self.settings = QSettings("BionicButterfly", "LinkMonitor")
        self._history = deque(maxlen=HISTORY_CAPACITY)
        self._last_plot = None
        self._last_render = None
        self._replay_engine = MetricsEngine()
        self._replay_iter = None
        self._replay_next = None
        self._replay_path: Path | None = None
        self._replay_running = False
        self._replay_elapsed = 0.0
        self._replay_anchor = time.monotonic()
        self._replay_warning: list[str] = []
        self._jobs: Queue = Queue()
        self._export_thread = None
        self._closing = False
        self._build_ui()
        self.refresh_ports()
        self.timer = QTimer(self)
        self.timer.setInterval(100)
        self.timer.timeout.connect(self.tick)
        self.timer.start()
        self.update_controls()

    def _build_ui(self):
        central = QWidget()
        root = QVBoxLayout(central)
        self.setCentralWidget(central)
        title = QLabel("LINK MONITOR  |  USB serial only • no radio commands")
        title.setStyleSheet("font-size:20px; font-weight:600; padding:8px")
        root.addWidget(title)
        warning = QLabel("Observasi agregat ~1 detik, bukan trace per paket. ACK coverage & gap adalah estimasi. "
                         "Membuka USB dapat mereset board pada driver tertentu.")
        warning.setWordWrap(True)
        root.addWidget(warning)
        ports = QHBoxLayout()
        self.port_combos = {}
        self.connect_buttons = {}
        self.port_status = {}
        for role, name in [("CTRL", "Controller"), ("RX", "Receiver")]:
            box = QGroupBox(name + " — 115200 baud")
            layout = QVBoxLayout(box)
            combo = QComboBox()
            combo.setEditable(True)
            combo.setMinimumWidth(200)
            layout.addWidget(combo)
            button = QPushButton("Connect")
            button.clicked.connect(lambda checked=False, r=role: self.toggle_connection(r))
            layout.addWidget(button)
            status = QLabel("Disconnected")
            status.setWordWrap(True)
            layout.addWidget(status)
            self.port_combos[role] = combo
            self.connect_buttons[role] = button
            self.port_status[role] = status
            ports.addWidget(box)
        self.refresh_button = QPushButton("Refresh ports")
        self.refresh_button.clicked.connect(self.refresh_ports)
        ports.addWidget(self.refresh_button)
        root.addLayout(ports)

        capture = QHBoxLayout()
        default_dir = str(Path.home() / "Documents" / "BionicButterfly" / "LinkMonitor")
        self.capture_dir = QLineEdit(str(self.settings.value("capture_dir", default_dir)))
        capture.addWidget(QLabel("Capture folder"))
        capture.addWidget(self.capture_dir, 1)
        self.browse_button = QPushButton("Browse")
        self.browse_button.clicked.connect(self.choose_dir)
        capture.addWidget(self.browse_button)
        self.start_button = QPushButton("Start")
        self.start_button.clicked.connect(self.start_capture)
        self.stop_button = QPushButton("Stop")
        self.stop_button.clicked.connect(self.stop_capture)
        capture.addWidget(self.start_button)
        capture.addWidget(self.stop_button)
        root.addLayout(capture)
        self.capture_status = QLabel("Connect kedua board dan tunggu stats sebelum Start.")
        self.capture_status.setWordWrap(True)
        root.addWidget(self.capture_status)

        flow = QHBoxLayout()
        self.nodes = []
        for text in ["Controller\nControl submitted", "Receiver\nTransport frames",
                     "Receiver\nValidated controls", "Receiver\nACK submitted", "Controller\nACK confirmed"]:
            label = QLabel(text + "\nN/A")
            label.setStyleSheet("background:#233444; color:#eaf1f8; border-radius:6px; padding:12px")
            flow.addWidget(label, 1)
            self.nodes.append((label, text))
            if len(self.nodes) < 5:
                flow.addWidget(QLabel("→"))
        root.addLayout(flow)
        self.summary = QLabel("ACK coverage: N/A | Unconfirmed gap: N/A | Expired: N/A")
        self.summary.setStyleSheet("font-weight:600; font-size:15px; padding:8px")
        root.addWidget(self.summary)
        self.quality = QLabel("Baseline belum tersedia.")
        self.quality.setWordWrap(True)
        self.quality.setStyleSheet("color:#ad6515")
        root.addWidget(self.quality)

        tabs = QTabWidget()
        root.addWidget(tabs, 1)
        chart_widget = QWidget()
        chart_layout = QVBoxLayout(chart_widget)
        self.charts = []
        for title, unit in [("ACK coverage — estimasi per segmen", "%"),
                            ("RTT ACK terakhir — bukan rata-rata", "ms"),
                            ("Control submitted / validated rate", "per second")]:
            chart = pg.PlotWidget(title=title)
            chart.setLabel("bottom", "Capture elapsed", units="s")
            chart.setLabel("left", unit)
            chart.showGrid(x=True, y=True, alpha=0.2)
            if unit == "per second":
                chart.addLegend()
            curve = chart.plot(pen=pg.mkPen("#48b8e8", width=2), name="Controller submitted")
            secondary = chart.plot(pen=pg.mkPen("#70d09a", width=2), name="Receiver validated") if unit == "per second" else None
            chart_layout.addWidget(chart)
            self.charts.append((curve, secondary))
        tabs.addTab(chart_widget, "Grafik")
        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Role / segment", "Metric", "Value", "Meaning"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        tabs.addTab(self.table, "Counters")
        self.raw_log = QPlainTextEdit()
        self.raw_log.setReadOnly(True)
        self.raw_log.setMaximumBlockCount(500)
        tabs.addTab(self.raw_log, "Serial log (last 500 lines)")

        replay = QHBoxLayout()
        self.load_button = QPushButton("Open replay JSONL")
        self.load_button.clicked.connect(self.open_replay)
        self.play_button = QPushButton("Play")
        self.play_button.clicked.connect(self.toggle_play)
        self.restart_button = QPushButton("Restart replay")
        self.restart_button.clicked.connect(self.restart_replay)
        self.exit_replay_button = QPushButton("Exit replay")
        self.exit_replay_button.clicked.connect(self.exit_replay)
        self.speed = QDoubleSpinBox()
        self.speed.setRange(0.25, 8)
        self.speed.setValue(1)
        self.speed.setSuffix(" ×")
        self.speed.valueChanged.connect(self.change_speed)
        self.export_button = QPushButton("Export CSV")
        self.export_button.clicked.connect(self.export_capture)
        for widget in [self.load_button, self.play_button, self.restart_button, self.exit_replay_button,
                       self.speed, self.export_button]:
            replay.addWidget(widget)
        root.addLayout(replay)

    def refresh_ports(self):
        try:
            choices = available_ports()
        except OSError as exc:
            self.show_error(str(exc))
            return
        for role, combo in self.port_combos.items():
            previous = combo.currentText().split(" — ")[0] or str(self.settings.value("port_" + role, ""))
            combo.clear()
            for device, description in choices:
                combo.addItem(f"{device} — {description}", device)
            index = combo.findData(previous)
            if index >= 0:
                combo.setCurrentIndex(index)
            elif previous:
                combo.setEditText(previous)

    def toggle_connection(self, role):
        try:
            if role in self.session.workers:
                self.session.disconnect(role)
            else:
                port = self.port_combos[role].currentText().split(" — ")[0].strip()
                self.settings.setValue("port_" + role, port)
                self.session.connect(role, port)
        except (ValueError, OSError) as exc:
            self.show_error(str(exc))
        self.update_controls()

    def choose_dir(self):
        directory = QFileDialog.getExistingDirectory(self, "Capture folder", self.capture_dir.text())
        if directory:
            self.capture_dir.setText(directory)

    def start_capture(self):
        try:
            directory = Path(self.capture_dir.text()).expanduser()
            directory.mkdir(parents=True, exist_ok=True)
            filename = "capture_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".jsonl"
            self.session.start(directory / filename)
            self.settings.setValue("capture_dir", str(directory))
            self._history.clear()
            self._last_plot = None
            self._last_render = None
            self.raw_log.clear()
        except (ValueError, OSError) as exc:
            self.show_error(str(exc))
        self.update_controls()

    def stop_capture(self):
        self.session.stop()
        self.update_controls()

    def show_error(self, message):
        QMessageBox.warning(self, "Link Monitor", message)

    def update_controls(self):
        replay = self._replay_path is not None
        self.start_button.setEnabled(not replay and self.session.can_start())
        self.stop_button.setEnabled(self.session.active)
        self.load_button.setEnabled(not self.session.active)
        for role, button in self.connect_buttons.items():
            button.setText("Disconnect" if role in self.session.workers else "Connect")
            button.setEnabled(not self.session.active and not replay)
            self.port_combos[role].setEnabled(role not in self.session.workers and not self.session.active and not replay)
        self.refresh_button.setEnabled(not self.session.active and not replay)
        self.browse_button.setEnabled(not self.session.active)
        self.capture_dir.setEnabled(not self.session.active)
        self.play_button.setEnabled(replay and self._replay_next is not None)
        self.play_button.setText("Pause" if self._replay_running else "Play")
        self.restart_button.setEnabled(replay)
        self.exit_replay_button.setEnabled(replay)
        self.speed.setEnabled(replay)
        self.export_button.setEnabled(not self.session.active and (replay or self.session.path is not None)
                                      and self._export_thread is None)

    def tick(self):
        records = self.session.poll()
        if self._replay_path is None:
            for record in records:
                self.raw_log.appendPlainText(f"[{record.role}] {record.raw}")
        else:
            self.advance_replay()
        now = time.monotonic()
        for role, status in self.port_status.items():
            worker = self.session.workers.get(role)
            if not worker:
                status.setText("Disconnected")
            elif worker.error:
                status.setText(worker.error)
            elif worker.connected:
                stale = now - self.session.last_stats.get(role, 0) >= LOG_STALE_SECONDS
                status.setText("Serial connected • " + ("stats stale / unknown radio state" if stale else
                               "role verified" if self.session.verified[role] else "waiting for role stats"))
            else:
                status.setText("Opening port…")
        if self._replay_path:
            self.capture_status.setText(f"REPLAY {'playing' if self._replay_running else 'paused / ended'} — {self._replay_path}")
            snapshot = self._replay_engine.snapshot()
            snapshot["quality"] = snapshot.get("quality", []) + self._replay_warning
            elapsed = self._replay_elapsed
        else:
            snapshot = self.session.snapshot()
            if self.session.active:
                ready = all(snapshot["roles"][r].get("ready") for r in ("CTRL", "RX"))
                self.capture_status.setText(f"CAPTURING {'— baseline ready' if ready else '— waiting for fresh baselines'} — {self.session.path}")
            elif self.session.path:
                self.capture_status.setText(f"STOPPED {'INCOMPLETE' if self.session.incomplete else '(boundary estimate)'} — {self.session.path}")
            else:
                self.capture_status.setText("Connect kedua board dan tunggu stats sebelum Start.")
            elapsed = max((r.elapsed for r in records), default=0)
        self.render_snapshot(snapshot, elapsed)
        self.update_controls()
        try:
            result, error = self._jobs.get_nowait()
        except Empty:
            return
        self._export_thread = None
        if error:
            self.show_error(error)
        else:
            self.statusBar().showMessage(f"CSV saved: {result}", 10000)

    def render_snapshot(self, snapshot, elapsed):
        ctrl = snapshot["roles"].get("CTRL", {})
        rx = snapshot["roles"].get("RX", {})
        c = ctrl.get("totals", {}) if ctrl.get("ready") else {}
        r = rx.get("totals", {}) if rx.get("ready") else {}
        values = [c.get("controls"), r.get("rx_frames"), r.get("controls"), r.get("acks"), c.get("confirmed_acks")]
        for (label, text), value in zip(self.nodes, values):
            label.setText(text + "\n" + display(value))
        coverage = ctrl.get("ack_coverage")
        self.summary.setText(f"ACK coverage: {display(coverage)}{'%' if coverage is not None else ''} | "
                             f"Unconfirmed gap (estimasi): {display(ctrl.get('unconfirmed'))} | "
                             f"Expired: {display(r.get('expired'))}")
        quality = snapshot.get("quality", [])
        self.quality.setText(" | ".join(quality) if quality else "Counter lintas board tidak tersinkron. Semua hasil berbatas capture; MAC success ≠ application ACK.")
        render_token = (repr(snapshot.get("roles", {})), tuple(quality))
        if render_token != self._last_render:
            rows = []
            for role, data in [("CTRL", ctrl), ("RX", rx)]:
                for group, meaning in [("totals", "delta within segment"), ("gauges", "last observed / gauge"),
                                       ("status", "firmware status"), ("rates", "per second (board clock)")]:
                    for key, value in data.get(group, {}).items():
                        rows.append([f"{role} / {data.get('segment', 0)}", key, display(value), meaning])
            self.table.setRowCount(len(rows))
            for index, row in enumerate(rows):
                for column, text in enumerate(row):
                    self.table.setItem(index, column, QTableWidgetItem(text))
            self.table.resizeColumnsToContents()
            self._last_render = render_token
        token = (ctrl.get("segment"), rx.get("segment"), tuple(values),
                 ctrl.get("gauges", {}).get("ack_rtt_ms"),
                 ctrl.get("status", {}).get("t"), rx.get("status", {}).get("t"))
        if token == self._last_plot:
            return
        if elapsed > 0:
            if self._last_plot and token[:2] != self._last_plot[:2]:
                self._history.append((elapsed, None, None, None, None))
            self._history.append((elapsed, coverage, ctrl.get("gauges", {}).get("ack_rtt_ms"),
                                  ctrl.get("rates", {}).get("controls"), rx.get("rates", {}).get("controls")))
        self._last_plot = token
        x = [point[0] for point in self._history]
        def series(column):
            return [float("nan") if point[column] is None else point[column] for point in self._history]
        for index, (curve, secondary) in enumerate(self.charts):
            curve.setData(x, series(index + 1), connect="finite")
            if secondary:
                secondary.setData(x, series(4), connect="finite")

    def open_replay(self):
        filename, _ = QFileDialog.getOpenFileName(self, "Replay capture", str(Path.home()), "Capture (*.jsonl)")
        if not filename:
            return
        if self.session.workers:
            answer = QMessageBox.question(self, "Replay", "Disconnect both serial ports before replay?")
            if answer != QMessageBox.StandardButton.Yes:
                return
            for role in tuple(self.session.workers):
                self.session.disconnect(role)
        self.load_replay(Path(filename))

    def load_replay(self, path):
        if self.session.active or self.session.workers:
            raise ValueError("Stop capture and disconnect serial before replay")
        self.exit_replay()
        self._replay_path = Path(path)
        self._replay_iter = iter_records(self._replay_path)
        self._replay_engine.reset()
        self._history.clear()
        self._last_plot = None
        self._last_render = None
        self.raw_log.clear()
        self._replay_warning.clear()
        self._replay_elapsed = 0
        self._replay_anchor = time.monotonic()
        self._fetch_next()
        self._replay_running = self._replay_next is not None
        self.update_controls()

    def _fetch_next(self):
        try:
            with warnings.catch_warnings(record=True) as notices:
                warnings.simplefilter("always")
                self._replay_next = next(self._replay_iter, None)
            self._replay_warning.extend(str(item.message) for item in notices)
        except (OSError, ValueError) as exc:
            self._replay_warning.append(f"Replay incomplete: {exc}")
            self._replay_next = None
        if self._replay_next is None:
            self._replay_running = False

    def advance_replay(self):
        if not self._replay_running:
            return
        now = time.monotonic()
        self._replay_elapsed += (now - self._replay_anchor) * self.speed.value()
        self._replay_anchor = now
        for _ in range(128):
            if self._replay_next is None or self._replay_next.elapsed > self._replay_elapsed:
                break
            record = self._replay_next
            self._replay_engine.ingest(record)
            self.raw_log.appendPlainText(f"[{record.role}] {record.raw}")
            self._fetch_next()

    def toggle_play(self):
        self._replay_running = not self._replay_running and self._replay_next is not None
        self._replay_anchor = time.monotonic()
        self.update_controls()

    def change_speed(self):
        self._replay_anchor = time.monotonic()

    def restart_replay(self):
        if self._replay_path:
            self.load_replay(self._replay_path)

    def exit_replay(self):
        if self._replay_iter:
            self._replay_iter.close()
        had_replay = self._replay_path is not None
        self._replay_iter = None
        self._replay_next = None
        self._replay_path = None
        self._replay_running = False
        if had_replay:
            self._history.clear()
            self._last_plot = None
        self.update_controls()

    def export_capture(self):
        source = self._replay_path or self.session.path
        if not source:
            return
        filename, _ = QFileDialog.getSaveFileName(self, "Export snapshots CSV", str(source.with_suffix(".csv")), "CSV (*.csv)")
        if not filename:
            return
        def run():
            try:
                export_csv(source, Path(filename))
                self._jobs.put((filename, None))
            except (OSError, ValueError) as exc:
                self._jobs.put((None, str(exc)))
        self._export_thread = threading.Thread(target=run, name="csv-export", daemon=True)
        self._export_thread.start()
        self.update_controls()

    def closeEvent(self, event):
        if self._export_thread and self._export_thread.is_alive():
            self.statusBar().showMessage("Wait for CSV export to finish before closing")
            event.ignore()
            return
        self.timer.stop()
        self.session.close()
        self.exit_replay()
        event.accept()
