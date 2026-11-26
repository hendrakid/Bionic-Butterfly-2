from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only dual USB serial Link Monitor")
    parser.add_argument("--smoke-test", action="store_true", help="Open UI briefly without hardware, then exit")
    parser.add_argument("--smoke-output", type=Path, help="Write smoke-test result as JSON")
    args = parser.parse_args()
    if args.smoke_test:
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtCore import QCoreApplication, QEvent, QTimer
    from PySide6.QtWidgets import QApplication
    from .ui import MonitorWindow
    app = QApplication([sys.argv[0]])
    window = MonitorWindow()
    window.show()
    if args.smoke_test:
        def finish():
            result = {"ok": window.isVisible() and len(window.nodes) == 5,
                      "serial_connections": len(window.session.workers),
                      "charts": len(window.charts)}
            if args.smoke_output:
                args.smoke_output.write_text(json.dumps(result), encoding="utf-8")
            window.close()
            app.exit(0 if result["ok"] else 1)
        QTimer.singleShot(400, finish)
    result = app.exec()
    window.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
