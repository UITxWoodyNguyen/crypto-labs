"""PyQt6 GUI for the Lab 1 Crypto++ AES command-line tool.

Run from the lab1 directory:
    python3 gui/aestool_gui.py
"""

from __future__ import annotations

import shlex
import sys
from pathlib import Path

from PyQt6.QtCore import QProcess, QProcessEnvironment
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
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
        self.setWindowTitle("Lab 1 - Crypto++ AES")
        self.resize(980, 820)
        self._build_ui()

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        tool_box = QGroupBox("aestool executable")
        tool_layout = QHBoxLayout(tool_box)
        self.tool_path = QLineEdit(str(self._default_tool()))
        browse = QPushButton("Browse...")
        browse.clicked.connect(lambda: self._browse(self.tool_path, "Select aestool"))
        tool_layout.addWidget(self.tool_path, 1)
        tool_layout.addWidget(browse)
        root.addWidget(tool_box)

        crypto_box = QGroupBox("Encryption / decryption")
        form = QFormLayout(crypto_box)
        self.mode = QComboBox()
        self.mode.addItems(["ecb", "cbc", "cfb", "ofb", "ctr", "xts", "ccm", "gcm"])
        form.addRow("Mode", self.mode)
        self.key_path = self._file_row(form, "Key file")
        self.key_hex = QLineEdit()
        self.key_hex.setPlaceholderText("Optional hexadecimal key; used instead of Key file")
        form.addRow("Key hex", self.key_hex)
        self.iv_path = self._file_row(form, "IV / nonce file")
        self.iv_hex = QLineEdit()
        self.iv_hex.setPlaceholderText("Optional hexadecimal IV/nonce")
        form.addRow("IV / nonce hex", self.iv_hex)
        self.input_path = self._file_row(form, "Input file")
        self.text_input = QLineEdit()
        self.text_input.setPlaceholderText("Optional text; used instead of Input file")
        form.addRow("Text", self.text_input)
        self.output_path = self._file_row(form, "Output file", save=True)
        self.meta_path = self._file_row(form, "Metadata JSON", save=True)
        self.aad_path = self._file_row(form, "AAD file")
        self.aad_text = QLineEdit()
        form.addRow("AAD text", self.aad_text)
        self.tag_hex = QLineEdit()
        self.tag_hex.setPlaceholderText("Authentication tag for decryption")
        form.addRow("Tag hex", self.tag_hex)
        self.tag_length = QComboBox()
        self.tag_length.addItems(["4", "8", "16"])
        form.addRow("CCM tag length", self.tag_length)

        options = QGridLayout()
        self.aead = QCheckBox("--aead")
        self.allow_ecb = QCheckBox("--allow-ecb")
        self.verbose = QCheckBox("--verbose")
        self.hex_key = QCheckBox("Generate hex key")
        options.addWidget(self.aead, 0, 0)
        options.addWidget(self.allow_ecb, 0, 1)
        options.addWidget(self.verbose, 0, 2)
        options.addWidget(self.hex_key, 0, 3)
        self.encoding = QComboBox()
        self.encoding.addItems(["hex", "base64", "raw"])
        options.addWidget(QLabel("Output encoding"), 1, 0)
        options.addWidget(self.encoding, 1, 1)
        self.nonce_db = QLineEdit(".aestool_nonces")
        options.addWidget(QLabel("Nonce database"), 1, 2)
        options.addWidget(self.nonce_db, 1, 3)
        form.addRow("Options", options)
        root.addWidget(crypto_box)

        actions = QGridLayout()
        for index, (label, callback) in enumerate((
            ("Encrypt", self.encrypt),
            ("Decrypt", self.decrypt),
            ("Generate key", self.keygen),
            ("Run KAT", self.kat),
            ("Benchmark", self.bench),
        )):
            button = QPushButton(label)
            button.clicked.connect(callback)
            actions.addWidget(button, 0, index)
        root.addLayout(actions)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        root.addWidget(QLabel("Command output"))
        root.addWidget(self.log, 1)

    def _default_tool(self) -> Path:
        candidates = [
            LAB_ROOT / "build_ubuntu_gcc" / "aestool",
            LAB_ROOT / "build_ubuntu_clang" / "aestool",
            LAB_ROOT / "build_windows_gcc" / "aestool.exe",
            LAB_ROOT / "build_windows_clang" / "aestool.exe",
            LAB_ROOT / "build_windows_msvc" / "Release" / "aestool.exe",
        ]
        return next((path for path in candidates if path.exists()), candidates[0])

    def _file_row(self, form: QFormLayout, label: str, save: bool = False) -> QLineEdit:
        edit = QLineEdit()
        button = QPushButton("Browse...")
        button.clicked.connect(lambda: self._browse(edit, label, save))
        row = QHBoxLayout()
        row.addWidget(edit, 1)
        row.addWidget(button)
        form.addRow(label, row)
        return edit

    def _browse(self, edit: QLineEdit, title: str, save: bool = False) -> None:
        if save:
            path, _ = QFileDialog.getSaveFileName(self, title, str(LAB_ROOT))
        else:
            path, _ = QFileDialog.getOpenFileName(self, title, str(LAB_ROOT))
        if path:
            edit.setText(path)

    def _append_option(self, args: list[str], name: str, value: str) -> None:
        if value.strip():
            args.extend([name, value.strip()])

    def _common_args(self, command: str) -> list[str]:
        args = [command, "--mode", self.mode.currentText()]
        if self.key_hex.text().strip():
            self._append_option(args, "--key-hex", self.key_hex.text())
        elif self.key_path.text().strip():
            self._append_option(args, "--key", self.key_path.text())
        else:
            QMessageBox.warning(self, "Missing key", "Choose a key file or enter a hexadecimal key.")
            return []
        if self.iv_hex.text().strip():
            self._append_option(args, "--iv", self.iv_hex.text())
        elif self.iv_path.text().strip():
            self._append_option(args, "--iv", self.iv_path.text())
        if self.input_path.text().strip() and self.text_input.text():
            QMessageBox.warning(self, "Input conflict", "Use either Input file or Text, not both.")
            return []
        if self.input_path.text().strip():
            self._append_option(args, "--in", self.input_path.text())
        elif self.text_input.text():
            self._append_option(args, "--text", self.text_input.text())
        else:
            QMessageBox.warning(self, "Missing input", "Choose an input file or enter text.")
            return []
        self._append_option(args, "--out", self.output_path.text())
        self._append_option(args, "--meta", self.meta_path.text())
        self._append_option(args, "--aad", self.aad_path.text())
        self._append_option(args, "--aad-text", self.aad_text.text())
        self._append_option(args, "--tag-hex", self.tag_hex.text())
        self._append_option(args, "--nonce-db", self.nonce_db.text())
        if self.encoding.currentText() != "hex" or command == "decrypt":
            self._append_option(args, "--encode", self.encoding.currentText())
        if self.mode.currentText() == "ccm":
            self._append_option(args, "--tag-len", self.tag_length.currentText())
        for checkbox, flag in (
            (self.aead, "--aead"),
            (self.allow_ecb, "--allow-ecb"),
            (self.verbose, "--verbose"),
        ):
            if checkbox.isChecked():
                args.append(flag)
        return args

    def _start(self, args: list[str]) -> None:
        if self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning:
            QMessageBox.warning(self, "Busy", "Another aestool command is still running.")
            return
        tool = Path(self.tool_path.text().strip())
        if not tool.is_file():
            QMessageBox.critical(self, "Tool not found", f"Cannot find aestool:\n{tool}")
            return
        self.log.appendPlainText("$ " + shlex.join([str(tool), *args]))
        self.process = QProcess(self)
        self.process.setProgram(str(tool))
        self.process.setArguments(args)
        self.process.setWorkingDirectory(str(LAB_ROOT))
        environment = QProcessEnvironment.systemEnvironment()
        environment.insert("PYTHONIOENCODING", "utf-8")
        self.process.setProcessEnvironment(environment)
        self.process.readyReadStandardOutput.connect(self._read_output)
        self.process.readyReadStandardError.connect(self._read_error)
        self.process.finished.connect(
            lambda code, _status: self.log.appendPlainText(f"[process exited with code {code}]")
        )
        self.process.start()

    def _read_output(self) -> None:
        if self.process is not None:
            text = bytes(self.process.readAllStandardOutput()).decode("utf-8", errors="replace").rstrip()
            if text:
                self.log.appendPlainText(text)

    def _read_error(self) -> None:
        if self.process is not None:
            text = bytes(self.process.readAllStandardError()).decode("utf-8", errors="replace").rstrip()
            if text:
                self.log.appendPlainText(text)

    def encrypt(self) -> None:
        args = self._common_args("encrypt")
        if args:
            self._start(args)

    def decrypt(self) -> None:
        args = self._common_args("decrypt")
        if args:
            self._start(args)

    def keygen(self) -> None:
        directory = QFileDialog.getExistingDirectory(self, "Select output directory", str(LAB_ROOT))
        if not directory:
            return
        output, _ = QFileDialog.getSaveFileName(self, "Save generated key", directory)
        if output:
            args = ["keygen", "--bits", "256", "--out", output]
            if self.hex_key.isChecked():
                args.append("--hex")
            self._start(args)

    def kat(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Select KAT vectors", str(LAB_ROOT / "kat"), "JSON (*.json)"
        )
        if path:
            self._start(["--kat", path])

    def bench(self) -> None:
        output, _ = QFileDialog.getSaveFileName(
            self, "Save benchmark CSV", str(LAB_ROOT / "output" / "ubuntu" / "benchmarks"), "CSV (*.csv)"
        )
        if output:
            self._start(["bench", "--runs", "30", "--out", output, "--quick"])


def main() -> int:
    app = QApplication(sys.argv)
    window = AesToolWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
