"""Small PyQt6 GUI for the lab2 ``aestool`` command-line tool.

Run manually from the lab2 directory:
    python gui/aestool_gui.py

The GUI never runs a command during startup.  It only starts ``aestool`` after
the corresponding button is pressed.
"""

from __future__ import annotations

import sys
from pathlib import Path

from PyQt6.QtCore import QProcess, QProcessEnvironment
from PyQt6.QtWidgets import (
    QApplication,
    QFileDialog,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QVBoxLayout,
    QWidget,
)


LAB_ROOT = Path(__file__).resolve().parents[1]


class AesToolWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.process: QProcess | None = None
        self.setWindowTitle("Lab 2 - AES-128 CTR")
        self.resize(880, 720)
        self._build_ui()

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        tool_box = QGroupBox("aestool")
        tool_layout = QHBoxLayout(tool_box)
        self.tool_path = QLineEdit(str(self._default_tool()))
        browse_tool = QPushButton("Browse...")
        browse_tool.clicked.connect(lambda: self._browse_file(self.tool_path, "Select aestool"))
        tool_layout.addWidget(self.tool_path, 1)
        tool_layout.addWidget(browse_tool)
        root.addWidget(tool_box)

        io_box = QGroupBox("Crypt operation")
        io_layout = QFormLayout(io_box)
        self.key_path = self._path_row(io_layout, "Key file")
        self.iv_path = self._path_row(io_layout, "IV file")
        self.input_path = self._path_row(io_layout, "Input file")
        self.output_path = self._path_row(io_layout, "Output file", save=True)
        self.text_input = QLineEdit()
        self.text_input.setPlaceholderText("Optional UTF-8 text (used when Input file is empty)")
        io_layout.addRow("Text", self.text_input)
        self.backend = QLineEdit("table")
        self.backend.setPlaceholderText("table, ttable, or computed")
        io_layout.addRow("S-box", self.backend)
        root.addWidget(io_box)

        actions = QGridLayout()
        for column, (label, callback) in enumerate((
            ("Encrypt", self.encrypt),
            ("Decrypt", self.decrypt),
            ("Generate key", self.keygen),
            ("Generate IV", self.ivgen),
            ("Run KAT", self.kat),
            ("Cross-validate", self.xval),
            ("Benchmark", self.bench),
        )):
            button = QPushButton(label)
            button.clicked.connect(callback)
            actions.addWidget(button, column // 4, column % 4)
        root.addLayout(actions)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setPlaceholderText("Command output appears here.")
        root.addWidget(QLabel("Output"))
        root.addWidget(self.log, 1)

    def _default_tool(self) -> Path:
        candidates = [
            LAB_ROOT / "build_ubuntu_gcc" / "aestool",
            LAB_ROOT / "build_ubuntu_clang" / "aestool",
            LAB_ROOT / "build_windows_gcc" / "aestool.exe",
            LAB_ROOT / "build_windows_msvc" / "Release" / "aestool.exe",
        ]
        return next((path for path in candidates if path.exists()), candidates[0])

    def _path_row(self, form: QFormLayout, label: str, save: bool = False) -> QLineEdit:
        edit = QLineEdit()
        button = QPushButton("Browse...")
        button.clicked.connect(lambda: self._browse_file(edit, label, save))
        row = QHBoxLayout()
        row.addWidget(edit, 1)
        row.addWidget(button)
        form.addRow(label, row)
        return edit

    def _browse_file(self, edit: QLineEdit, title: str, save: bool = False) -> None:
        path, _ = (
            QFileDialog.getSaveFileName(self, title, str(LAB_ROOT))
            if save
            else QFileDialog.getOpenFileName(self, title, str(LAB_ROOT))
        )
        if path:
            edit.setText(path)

    def _start(self, arguments: list[str]) -> None:
        if self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning:
            QMessageBox.warning(self, "Busy", "Another aestool command is still running.")
            return
        tool = Path(self.tool_path.text().strip())
        if not tool.is_file():
            QMessageBox.critical(self, "Tool not found", f"Cannot find aestool:\n{tool}")
            return
        self.log.appendPlainText(f"$ {tool} {' '.join(arguments)}")
        self.process = QProcess(self)
        self.process.setWorkingDirectory(str(LAB_ROOT))
        environment = QProcessEnvironment.systemEnvironment()
        environment.insert("PYTHONIOENCODING", "utf-8")
        self.process.setProcessEnvironment(environment)
        self.process.setProgram(str(tool))
        self.process.setArguments(arguments)
        self.process.readyReadStandardOutput.connect(
            lambda: self.log.appendPlainText(bytes(self.process.readAllStandardOutput()).decode(errors="replace").rstrip())
        )
        self.process.readyReadStandardError.connect(
            lambda: self.log.appendPlainText(bytes(self.process.readAllStandardError()).decode(errors="replace").rstrip())
        )
        self.process.finished.connect(
            lambda code, status: self.log.appendPlainText(f"[process exited with code {code}]")
        )
        self.process.start()

    def _crypt_args(self, command: str) -> list[str]:
        args = [command, "--mode", "ctr", "--sbox", self.backend.text().strip() or "table", "--force"]
        if self.key_path.text().strip():
            args += ["--key", self.key_path.text().strip()]
        else:
            QMessageBox.warning(self, "Missing key", "Select a key file first.")
            return []
        if self.iv_path.text().strip():
            args += ["--iv", self.iv_path.text().strip()]
        if self.input_path.text().strip():
            args += ["--in", self.input_path.text().strip()]
        elif self.text_input.text():
            args += ["--text", self.text_input.text()]
        else:
            QMessageBox.warning(self, "Missing input", "Select an input file or enter text.")
            return []
        if self.output_path.text().strip():
            args += ["--out", self.output_path.text().strip()]
        return args

    def encrypt(self) -> None:
        if (args := self._crypt_args("encrypt")):
            self._start(args)

    def decrypt(self) -> None:
        if (args := self._crypt_args("decrypt")):
            self._start(args)

    def keygen(self) -> None:
        path = self._choose_output("Generate key")
        if path:
            self.key_path.setText(path)
            self._start(["keygen", "--out", path, "--force"])

    def ivgen(self) -> None:
        path = self._choose_output("Generate IV")
        if path:
            self.iv_path.setText(path)
            self._start(["ivgen", "--out", path, "--force"])

    def kat(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Select KAT JSON", str(LAB_ROOT / "kat"), "JSON (*.json)")
        if path:
            self._start(["--kat", path, "--verbose"])

    def xval(self) -> None:
        self._start(["xval", "--ref", "all", "--cases", "50"])

    def bench(self) -> None:
        self._start([
            "bench", "--sizes", "1MiB,100MiB", "--runs", "30",
            "--csv", str(LAB_ROOT / "Output" / "ubuntu" / "bench" / "gui_bench.csv"),
        ])

    def _choose_output(self, title: str) -> str:
        path, _ = QFileDialog.getSaveFileName(self, title, str(LAB_ROOT / "Output" / "ubuntu" / "demo"))
        return path


def main() -> int:
    app = QApplication(sys.argv)
    window = AesToolWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
