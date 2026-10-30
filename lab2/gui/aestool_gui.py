"""PyQt6 GUI for the Lab 2 AES-128 CTR command-line tool (aestool).

Run from the lab2 directory:
    python3 gui/aestool_gui.py

The GUI never runs a command during startup. It only starts aestool after you
press the matching button.

Highlights:
  * One shared form for encrypt / decrypt with a segmented switch.
  * S-box backend picker, file inputs with drag & drop, live size hints.
  * The exact command line is previewed live and can be copied.
  * Tools tab: key / IV generation, KAT, cross-validation, benchmark.
  * Colored log, background process with Stop, saved settings, light / dark theme.

Rendering note: text boxes, combo boxes and spin boxes use the native Fusion style
driven by a QPalette (robust). The stylesheet only styles cards, labels,
primary / segmented buttons, checkboxes, tabs and the log.
"""

from __future__ import annotations

import html
import os
import shlex
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from PyQt6.QtCore import (
    Qt,
    QProcess,
    QProcessEnvironment,
    QSettings,
    QTimer,
    QUrl,
    pyqtSignal,
)
from PyQt6.QtGui import (
    QColor,
    QDesktopServices,
    QFontDatabase,
    QGuiApplication,
    QKeySequence,
    QPalette,
    QShortcut,
)
from PyQt6.QtWidgets import (
    QApplication,
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QSplitter,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)


LAB_ROOT = Path(__file__).resolve().parents[1]

SBOX_BACKENDS: dict[str, str] = {
    "table": "table: SubBytes through a precomputed S-box lookup table.",
    "ttable": "ttable: combined T-table implementation of the round function.",
    "computed": "computed: S-box values are computed instead of looked up.",
}


# --------------------------------------------------------------------------- #
# Theme
# --------------------------------------------------------------------------- #

THEMES: dict[str, dict[str, str]] = {
    "light": {
        "bg": "#eef2f5",
        "card": "#ffffff",
        "input": "#ffffff",
        "text": "#18222d",
        "muted": "#66737f",
        "border": "#d3dae1",
        "accent": "#0f6e7a",
        "accent_hover": "#0b5862",
        "accent_text": "#ffffff",
        "ok": "#17803d",
        "warn": "#a8590a",
        "error": "#b42318",
        "warn_bg": "#fff6e5",
        "log_bg": "#f8fafb",
        "log_text": "#18222d",
        "cmd": "#0b5862",
    },
    "dark": {
        "bg": "#10171e",
        "card": "#19222c",
        "input": "#121a22",
        "text": "#e3e9ef",
        "muted": "#8a98a6",
        "border": "#2b3745",
        "accent": "#4aa6b4",
        "accent_hover": "#63b9c6",
        "accent_text": "#08121a",
        "ok": "#5fd08a",
        "warn": "#f0b35a",
        "error": "#f48b80",
        "warn_bg": "#2b2416",
        "log_bg": "#0c1218",
        "log_text": "#d5dde5",
        "cmd": "#7cc9d4",
    },
}

QSS_TEMPLATE = """
QToolTip { background: @card; color: @text; border: 1px solid @border; padding: 4px 6px; }

QFrame#card { background: @card; border: 1px solid @border; border-radius: 8px; }
QLabel#appTitle { font-size: 20px; font-weight: 700; }
QLabel#appSubtitle { color: @muted; }
QLabel#cardTitle { font-size: 14px; font-weight: 600; }
QLabel[role="hint"] { color: @muted; font-size: 12px; }
QLabel[role="hint"][state="ok"] { color: @ok; }
QLabel[role="hint"][state="warn"] { color: @warn; }
QLabel[role="hint"][state="error"] { color: @error; }

QPushButton[role="primary"] {
    background: @accent; color: @accent_text; border: 1px solid @accent;
    border-radius: 6px; padding: 7px 22px; font-weight: 600;
}
QPushButton[role="primary"]:hover { background: @accent_hover; border-color: @accent_hover; }
QPushButton[role="primary"]:pressed { background: @accent_hover; }
QPushButton[role="primary"]:disabled { background: @border; border-color: @border; color: @muted; }

QPushButton[role="danger"] {
    background: @card; color: @error; border: 1px solid @error;
    border-radius: 6px; padding: 6px 16px;
}
QPushButton[role="danger"]:hover { background: @warn_bg; }
QPushButton[role="danger"]:disabled { color: @muted; border-color: @border; }

QPushButton[seg] {
    background: @card; color: @text; border: 1px solid @border;
    padding: 6px 24px; border-radius: 6px;
}
QPushButton[seg]:hover { border-color: @accent; }
QPushButton[seg="left"] { border-top-right-radius: 0; border-bottom-right-radius: 0; }
QPushButton[seg="right"] {
    border-top-left-radius: 0; border-bottom-left-radius: 0; border-left-color: @accent;
}
QPushButton[seg]:checked {
    background: @accent; color: @accent_text; border-color: @accent; font-weight: 600;
}

QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border: 1px solid @muted;
    border-radius: 4px; background: @input;
}
QCheckBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
QCheckBox::indicator:disabled { border-color: @border; }

QTabWidget::pane { border: none; }
QTabBar::tab {
    background: transparent; color: @muted; padding: 8px 18px;
    border: none; border-bottom: 2px solid transparent; margin-right: 4px;
}
QTabBar::tab:hover { color: @text; }
QTabBar::tab:selected { color: @accent; border-bottom: 2px solid @accent; font-weight: 600; }

QSplitter::handle:vertical { height: 8px; background: transparent; }

QPlainTextEdit#log {
    background: @log_bg; color: @log_text; border: 1px solid @border;
    border-radius: 8px; padding: 8px;
}
"""


def build_qss(theme: str) -> str:
    qss = QSS_TEMPLATE
    palette = THEMES[theme]
    for key in sorted(palette, key=len, reverse=True):  # keep @accent from eating @accent_hover
        qss = qss.replace("@" + key, palette[key])
    return qss


def build_palette(theme: str) -> QPalette:
    c = THEMES[theme]
    palette = QPalette()
    role, group = QPalette.ColorRole, QPalette.ColorGroup
    for r, value in (
        (role.Window, c["bg"]),
        (role.WindowText, c["text"]),
        (role.Base, c["input"]),
        (role.AlternateBase, c["card"]),
        (role.Text, c["text"]),
        (role.Button, c["card"]),
        (role.ButtonText, c["text"]),
        (role.ToolTipBase, c["card"]),
        (role.ToolTipText, c["text"]),
        (role.Highlight, c["accent"]),
        (role.HighlightedText, c["accent_text"]),
        (role.PlaceholderText, c["muted"]),
        (role.Link, c["accent"]),
    ):
        palette.setColor(r, QColor(value))
    for r in (role.WindowText, role.Text, role.ButtonText):
        palette.setColor(group.Disabled, r, QColor(c["muted"]))
    palette.setColor(group.Disabled, role.Base, QColor(c["bg"]))
    palette.setColor(group.Disabled, role.Button, QColor(c["bg"]))
    return palette


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #


def format_command(parts: list[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(parts)
    return shlex.join(parts)


def set_hint(label: QLabel, text: str, state: str = "muted") -> None:
    label.setText(text)
    label.setVisible(bool(text))
    if label.property("state") != state:
        label.setProperty("state", state)
        label.style().unpolish(label)
        label.style().polish(label)


class HintLabel(QLabel):
    """Word-wrapped label that sizes its own height to the current width.

    Qt can under-estimate the height of wrapped labels inside nested layouts
    in a scroll area, which clips the text. This keeps the height exact.
    """

    def setText(self, text: str) -> None:  # noqa: N802
        super().setText(text)
        self._fit()

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        self._fit()

    def _fit(self) -> None:
        width = self.width()
        if width <= 40 or not self.wordWrap():
            return
        rect = self.fontMetrics().boundingRect(
            0, 0, width, 10000, int(Qt.TextFlag.TextWordWrap), self.text()
        )
        height = rect.height() + 2
        if self.minimumHeight() != height:
            self.setMinimumHeight(height)


def make_hint() -> QLabel:
    label = HintLabel("")
    label.setProperty("role", "hint")
    label.setWordWrap(True)
    label.setVisible(False)
    return label


def stack(*items: QWidget) -> QVBoxLayout:
    """Stack widgets in a layout (e.g. a hint under a field) so wrapped labels keep their height."""
    layout = QVBoxLayout()
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(4)
    for item in items:
        layout.addWidget(item)
    return layout


def make_card(title: str) -> tuple[QFrame, QFormLayout]:
    frame = QFrame()
    frame.setObjectName("card")
    outer = QVBoxLayout(frame)
    outer.setContentsMargins(16, 14, 16, 16)
    outer.setSpacing(12)
    heading = QLabel(title)
    heading.setObjectName("cardTitle")
    outer.addWidget(heading)
    form = QFormLayout()
    form.setLabelAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    form.setFieldGrowthPolicy(QFormLayout.FieldGrowthPolicy.ExpandingFieldsGrow)
    form.setHorizontalSpacing(14)
    form.setVerticalSpacing(10)
    outer.addLayout(form)
    return frame, form


def right_aligned(button: QPushButton) -> QWidget:
    holder = QWidget()
    row = QHBoxLayout(holder)
    row.setContentsMargins(0, 2, 0, 0)
    row.addStretch(1)
    row.addWidget(button)
    return holder


def primary_button(text: str, callback) -> QPushButton:
    button = QPushButton(text)
    button.setProperty("role", "primary")
    button.clicked.connect(callback)
    return button


# --------------------------------------------------------------------------- #
# Reusable widgets
# --------------------------------------------------------------------------- #


class DropLineEdit(QLineEdit):
    """QLineEdit that accepts files dropped onto it."""

    fileDropped = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()
        self.setAcceptDrops(True)

    def dragEnterEvent(self, event):  # noqa: N802 (Qt naming)
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            super().dragEnterEvent(event)

    def dropEvent(self, event):  # noqa: N802
        urls = event.mimeData().urls()
        if urls and urls[0].isLocalFile():
            self.fileDropped.emit(urls[0].toLocalFile())
            event.acceptProposedAction()
        else:
            super().dropEvent(event)


class SourceField(QWidget):
    """Input with an optional source picker (File / Text) and a Browse button."""

    changed = pyqtSignal()

    def __init__(
        self,
        kinds: list[tuple[str, str]],
        placeholders: dict[str, str] | None = None,
        title: str = "Select file",
        start_dir: Path = LAB_ROOT,
        file_filter: str = "",
    ) -> None:
        super().__init__()
        self._placeholders = placeholders or {}
        self._title = title
        self._start_dir = start_dir
        self._filter = file_filter

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)

        self.selector = QComboBox()
        for label, kind in kinds:
            self.selector.addItem(label, kind)
        self.selector.setVisible(len(kinds) > 1)

        self.edit = DropLineEdit()
        self.edit.setMinimumWidth(160)
        self.browse = QPushButton("Browse…")
        self.browse.setToolTip("Choose a file (or drop one onto the box)")

        layout.addWidget(self.selector)
        layout.addWidget(self.edit, 1)
        layout.addWidget(self.browse)

        self.selector.currentIndexChanged.connect(self._sync_kind)
        self.edit.textChanged.connect(self.changed)
        self.edit.fileDropped.connect(self._on_drop)
        self.browse.clicked.connect(self._browse)
        self._sync_kind()

    def kind(self) -> str:
        return str(self.selector.currentData())

    def text(self) -> str:
        return self.edit.text().strip()

    def is_file(self) -> bool:
        return self.kind() in ("file", "save")

    def set_value(self, kind: str, text: str) -> None:
        index = self.selector.findData(kind)
        if index >= 0:
            self.selector.setCurrentIndex(index)
        self.edit.setText(text)

    def _sync_kind(self) -> None:
        self.edit.setPlaceholderText(self._placeholders.get(self.kind(), ""))
        self.browse.setVisible(self.is_file())
        self.changed.emit()

    def _on_drop(self, path: str) -> None:
        if not self.is_file():
            index = self.selector.findData("file")
            if index >= 0:
                self.selector.setCurrentIndex(index)
        self.edit.setText(path)

    def _browse(self) -> None:
        start = self.text() or str(self._start_dir)
        if self.kind() == "save":
            path, _ = QFileDialog.getSaveFileName(self, self._title, start, self._filter)
        else:
            path, _ = QFileDialog.getOpenFileName(self, self._title, start, self._filter)
        if path:
            self.edit.setText(path)


# --------------------------------------------------------------------------- #
# Main window
# --------------------------------------------------------------------------- #


class AesToolWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.settings = QSettings("Lab2", "aestool_gui")
        self.theme = self.settings.value("theme", "light", type=str)
        if self.theme not in THEMES:
            self.theme = "light"
        self.process: QProcess | None = None
        self.run_buttons: list[QPushButton] = []
        self.last_output: Path | None = None
        self._pending_output: Path | None = None
        self._after = None
        self._cancelled = False
        self._ready = False
        self._log_entries: list[tuple[str, str]] = []

        self.setWindowTitle("Lab 2 - AES-128 CTR")
        self.resize(1120, 880)
        self.setMinimumSize(920, 660)
        self._build_ui()
        self._build_shortcuts()
        self._restore_settings()
        self._apply_theme()
        self._ready = True
        self._refresh()
        self._update_tool_status()

    # ------------------------------------------------------------------ UI --

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(18, 14, 18, 8)
        root.setSpacing(10)

        top = QHBoxLayout()
        titles = QVBoxLayout()
        titles.setSpacing(0)
        title = QLabel("AES-128 CTR")
        title.setObjectName("appTitle")
        subtitle = QLabel("GUI for aestool, Lab 2")
        subtitle.setObjectName("appSubtitle")
        titles.addWidget(title)
        titles.addWidget(subtitle)
        top.addLayout(titles)
        top.addStretch(1)
        self.theme_btn = QPushButton()
        self.theme_btn.setToolTip("Switch between light and dark theme")
        self.theme_btn.clicked.connect(self._toggle_theme)
        top.addWidget(self.theme_btn)
        root.addLayout(top)

        root.addWidget(self._build_tool_bar())

        splitter = QSplitter(Qt.Orientation.Vertical)
        splitter.setChildrenCollapsible(False)
        self.splitter = splitter
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        self.tabs.addTab(self._build_crypto_tab(), "Encrypt / Decrypt")
        self.tabs.addTab(self._build_tools_tab(), "Tools")
        splitter.addWidget(self.tabs)
        splitter.addWidget(self._build_log_panel())
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        root.addWidget(splitter, 1)

        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setTextVisible(False)
        self.progress.setFixedSize(140, 10)
        self.progress.setVisible(False)
        self.statusBar().addPermanentWidget(self.progress)
        self.statusBar().showMessage("Ready")

    def _build_tool_bar(self) -> QFrame:
        frame = QFrame()
        frame.setObjectName("card")
        row = QHBoxLayout(frame)
        row.setContentsMargins(14, 10, 14, 10)
        row.setSpacing(10)
        row.addWidget(QLabel("aestool"))
        self.tool_path = QLineEdit(str(self._default_tool()))
        self.tool_path.textChanged.connect(self._update_tool_status)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse_tool)
        self.tool_status = make_hint()
        self.tool_status.setWordWrap(False)
        row.addWidget(self.tool_path, 1)
        row.addWidget(browse)
        row.addWidget(self.tool_status)
        return frame

    def _scroll_page(self, body: QWidget) -> QScrollArea:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setWidget(body)
        return scroll

    def _build_crypto_tab(self) -> QWidget:
        page = QWidget()
        outer = QVBoxLayout(page)
        outer.setContentsMargins(0, 10, 0, 0)
        outer.setSpacing(10)

        body = QWidget()
        columns = QHBoxLayout(body)
        columns.setContentsMargins(0, 0, 8, 0)
        columns.setSpacing(12)
        left, right = QVBoxLayout(), QVBoxLayout()
        left.setSpacing(12)
        right.setSpacing(12)

        # --- Operation
        card, form = make_card("Operation")
        seg = QWidget()
        seg_row = QHBoxLayout(seg)
        seg_row.setContentsMargins(0, 0, 0, 0)
        seg_row.setSpacing(0)
        self.enc_btn = QPushButton("Encrypt")
        self.dec_btn = QPushButton("Decrypt")
        for button, side in ((self.enc_btn, "left"), (self.dec_btn, "right")):
            button.setCheckable(True)
            button.setProperty("seg", side)
            seg_row.addWidget(button)
        seg_row.addStretch(1)
        self.enc_btn.setChecked(True)
        group = QButtonGroup(self)
        group.setExclusive(True)
        group.addButton(self.enc_btn)
        group.addButton(self.dec_btn)
        form.addRow("Action", seg)

        mode_label = QLabel("AES-128, CTR (fixed)")
        form.addRow("Mode", mode_label)
        self.sbox = QComboBox()
        self.sbox.addItems(list(SBOX_BACKENDS))
        self.sbox_desc = make_hint()
        form.addRow("S-box", stack(self.sbox, self.sbox_desc))
        left.addWidget(card)

        # --- Key & IV
        card, form = make_card("Key and IV")
        self.key = SourceField(
            [("File", "file")], {"file": "Path to key file (AES-128)"}, title="Select key file"
        )
        self.key_hint = make_hint()
        self.iv = SourceField(
            [("File", "file")], {"file": "Path to IV file"}, title="Select IV file"
        )
        self.iv_hint = make_hint()
        form.addRow("Key file", stack(self.key, self.key_hint))
        form.addRow("IV file", stack(self.iv, self.iv_hint))
        left.addWidget(card)
        left.addStretch(1)

        # --- Data
        card, form = make_card("Data")
        self.data_in = SourceField(
            [("File", "file"), ("Text", "text")],
            {"file": "File to process (drop a file here)", "text": "Type UTF-8 text directly"},
            title="Select input file",
        )
        self.data_out = SourceField(
            [("File", "save")], {"save": "Optional - where to save the result"},
            title="Save output",
        )
        form.addRow("Input", self.data_in)
        form.addRow("Output", self.data_out)
        right.addWidget(card)

        # --- Options
        card, form = make_card("Options")
        self.force = QCheckBox("Overwrite existing output (--force)")
        self.force.setChecked(True)
        form.addRow(self.force)
        right.addWidget(card)
        right.addStretch(1)

        columns.addLayout(left, 1)
        columns.addLayout(right, 1)
        outer.addWidget(self._scroll_page(body), 1)
        outer.addWidget(self._build_action_bar())

        self.sbox.currentIndexChanged.connect(self._refresh)
        self.enc_btn.toggled.connect(self._on_op_changed)
        for field in (self.key, self.iv, self.data_in, self.data_out):
            field.changed.connect(self._refresh)
        self.force.toggled.connect(self._refresh)
        return page

    def _build_action_bar(self) -> QFrame:
        frame = QFrame()
        frame.setObjectName("card")
        row = QHBoxLayout(frame)
        row.setContentsMargins(14, 10, 14, 10)
        row.setSpacing(8)
        self.preview = QLineEdit()
        self.preview.setReadOnly(True)
        self.preview.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        copy_btn = QPushButton("Copy command")
        copy_btn.clicked.connect(self._copy_command)
        self.stop_btn = QPushButton("Stop")
        self.stop_btn.setProperty("role", "danger")
        self.stop_btn.setEnabled(False)
        self.stop_btn.setToolTip("Stop the running process (Ctrl+.)")
        self.stop_btn.clicked.connect(self._cancel)
        self.run_btn = primary_button("Encrypt", self.run_crypto)
        self.run_btn.setToolTip("Run (Ctrl+Enter)")
        self.run_buttons.append(self.run_btn)
        row.addWidget(self.preview, 1)
        row.addWidget(copy_btn)
        row.addWidget(self.stop_btn)
        row.addWidget(self.run_btn)
        return frame

    def _build_tools_tab(self) -> QWidget:
        body = QWidget()
        columns = QHBoxLayout(body)
        columns.setContentsMargins(0, 10, 8, 0)
        columns.setSpacing(12)
        left, right = QVBoxLayout(), QVBoxLayout()
        left.setSpacing(12)
        right.setSpacing(12)

        # Generate key
        card, form = make_card("Generate key")
        self.keygen_out = SourceField(
            [("File", "save")], {"save": "Where to save the key"}, title="Save key",
            start_dir=LAB_ROOT / "Output" / "ubuntu" / "demo",
        )
        form.addRow("Save to", self.keygen_out)
        form.addRow(right_aligned(self._tool_button("Generate key", self.run_keygen)))
        left.addWidget(card)

        # Generate IV
        card, form = make_card("Generate IV")
        self.ivgen_out = SourceField(
            [("File", "save")], {"save": "Where to save the IV"}, title="Save IV",
            start_dir=LAB_ROOT / "Output" / "ubuntu" / "demo",
        )
        form.addRow("Save to", self.ivgen_out)
        form.addRow(right_aligned(self._tool_button("Generate IV", self.run_ivgen)))
        left.addWidget(card)

        # KAT
        card, form = make_card("Known answer tests (KAT)")
        self.kat_path = SourceField(
            [("File", "file")],
            {"file": "KAT vectors JSON, e.g. kat/….json"},
            title="Select KAT JSON",
            start_dir=LAB_ROOT / "kat",
            file_filter="JSON (*.json)",
        )
        self.kat_verbose = QCheckBox("Verbose output (--verbose)")
        self.kat_verbose.setChecked(True)
        form.addRow("Vectors", self.kat_path)
        form.addRow(self.kat_verbose)
        form.addRow(right_aligned(self._tool_button("Run KAT", self.run_kat)))
        left.addWidget(card)
        left.addStretch(1)

        # Cross-validate
        card, form = make_card("Cross-validate")
        self.xval_ref = QLineEdit("all")
        self.xval_ref.setToolTip("Reference implementation to compare against (--ref)")
        self.xval_cases = QSpinBox()
        self.xval_cases.setRange(1, 1_000_000)
        self.xval_cases.setValue(50)
        form.addRow("Reference", self.xval_ref)
        form.addRow("Test cases", self.xval_cases)
        form.addRow(right_aligned(self._tool_button("Cross-validate", self.run_xval)))
        right.addWidget(card)

        # Benchmark
        card, form = make_card("Benchmark")
        self.bench_sizes = QLineEdit("1MiB,100MiB")
        self.bench_sizes.setToolTip("Comma-separated data sizes, e.g. 1MiB,100MiB")
        self.bench_runs = QSpinBox()
        self.bench_runs.setRange(1, 10_000)
        self.bench_runs.setValue(30)
        default_csv = LAB_ROOT / "Output" / "ubuntu" / "bench" / "gui_bench.csv"
        self.bench_out = SourceField(
            [("File", "save")],
            {"save": "Where to save the CSV results"},
            title="Save benchmark CSV",
            start_dir=default_csv.parent,
            file_filter="CSV (*.csv)",
        )
        self.bench_out.edit.setText(str(default_csv))
        form.addRow("Sizes", self.bench_sizes)
        form.addRow("Runs", self.bench_runs)
        form.addRow("CSV file", self.bench_out)
        form.addRow(right_aligned(self._tool_button("Run benchmark", self.run_bench)))
        right.addWidget(card)
        right.addStretch(1)

        columns.addLayout(left, 1)
        columns.addLayout(right, 1)
        return self._scroll_page(body)

    def _tool_button(self, text: str, callback) -> QPushButton:
        button = primary_button(text, callback)
        self.run_buttons.append(button)
        return button

    def _build_log_panel(self) -> QWidget:
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 4, 0, 0)
        layout.setSpacing(6)
        header = QHBoxLayout()
        header.addWidget(QLabel("Output"))
        header.addStretch(1)
        self.open_btn = QPushButton("Open output folder")
        self.open_btn.setEnabled(False)
        self.open_btn.clicked.connect(self._open_output_folder)
        copy_log = QPushButton("Copy log")
        copy_log.clicked.connect(
            lambda: QGuiApplication.clipboard().setText(self.log.toPlainText())
        )
        save_log = QPushButton("Save log…")
        save_log.clicked.connect(self._save_log)
        clear_log = QPushButton("Clear")
        clear_log.setToolTip("Clear the log (Ctrl+L)")
        clear_log.clicked.connect(self._clear_log)
        for button in (self.open_btn, copy_log, save_log, clear_log):
            header.addWidget(button)
        layout.addLayout(header)

        self.log = QPlainTextEdit()
        self.log.setObjectName("log")
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(5000)
        self.log.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self.log.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        self.log.setPlaceholderText("Command output appears here.")
        layout.addWidget(self.log, 1)
        return panel

    def _build_shortcuts(self) -> None:
        for sequence, callback in (
            ("Ctrl+Return", self.run_crypto),
            ("Ctrl+Enter", self.run_crypto),
            ("Ctrl+L", self._clear_log),
            ("Ctrl+.", self._cancel),
        ):
            QShortcut(QKeySequence(sequence), self).activated.connect(callback)

    # ------------------------------------------------------------ Settings --

    def _restore_settings(self) -> None:
        saved_tool = self.settings.value("tool_path", "", type=str)
        if saved_tool and Path(saved_tool).is_file():
            self.tool_path.setText(saved_tool)
        backend = self.settings.value("sbox", "table", type=str)
        if backend in SBOX_BACKENDS:
            self.sbox.setCurrentText(backend)
        geometry = self.settings.value("geometry")
        if geometry:
            self.restoreGeometry(geometry)
        sizes = self.settings.value("splitter")
        if sizes:
            self.splitter.restoreState(sizes)

    def closeEvent(self, event) -> None:  # noqa: N802
        self.settings.setValue("tool_path", self.tool_path.text().strip())
        self.settings.setValue("theme", self.theme)
        self.settings.setValue("sbox", self.sbox.currentText())
        self.settings.setValue("geometry", self.saveGeometry())
        self.settings.setValue("splitter", self.splitter.saveState())
        if self._is_running():
            self.process.kill()
        super().closeEvent(event)

    # --------------------------------------------------------------- Theme --

    def _apply_theme(self) -> None:
        QApplication.instance().setPalette(build_palette(self.theme))
        self.setStyleSheet(build_qss(self.theme))
        preview_palette = QApplication.palette()
        preview_palette.setColor(QPalette.ColorRole.Text, QColor(THEMES[self.theme]["cmd"]))
        self.preview.setPalette(preview_palette)
        self.theme_btn.setText("Dark theme" if self.theme == "light" else "Light theme")
        self._rerender_log()

    def _toggle_theme(self) -> None:
        self.theme = "dark" if self.theme == "light" else "light"
        self._apply_theme()

    # ----------------------------------------------------------- Tool path --

    def _default_tool(self) -> Path:
        candidates = [
            LAB_ROOT / "build_ubuntu_gcc" / "aestool",
            LAB_ROOT / "build_ubuntu_clang" / "aestool",
            LAB_ROOT / "build_windows_gcc" / "aestool.exe",
            LAB_ROOT / "build_windows_msvc" / "Release" / "aestool.exe",
        ]
        return next((path for path in candidates if path.exists()), candidates[0])

    def _browse_tool(self) -> None:
        start = self.tool_path.text().strip() or str(LAB_ROOT)
        path, _ = QFileDialog.getOpenFileName(self, "Select aestool", start)
        if path:
            self.tool_path.setText(path)

    def _update_tool_status(self) -> None:
        if Path(self.tool_path.text().strip()).is_file():
            set_hint(self.tool_status, "Found", "ok")
        else:
            set_hint(self.tool_status, "Not found", "error")

    # ---------------------------------------------------------- Form state --

    @property
    def op(self) -> str:
        return "encrypt" if self.enc_btn.isChecked() else "decrypt"

    def _on_op_changed(self) -> None:
        self.run_btn.setText("Encrypt" if self.op == "encrypt" else "Decrypt")
        self._refresh()

    def _refresh(self) -> None:
        if not self._ready:
            return
        set_hint(self.sbox_desc, SBOX_BACKENDS[self.sbox.currentText()])
        self._file_size_hint(self.key, self.key_hint, "AES-128 key", required=True)
        self._file_size_hint(self.iv, self.iv_hint, "IV", required=self.op == "decrypt")

        args, errors = self._crypto_args()
        if errors:
            self.preview.setText("Incomplete: " + errors[0])
        else:
            tool_name = Path(self.tool_path.text().strip()).name or "aestool"
            self.preview.setText(format_command([tool_name, *args]))
        self.preview.setCursorPosition(0)

    def _file_size_hint(self, field: SourceField, label: QLabel, what: str, required: bool) -> None:
        text = field.text()
        if not text:
            if what == "IV":
                if required:
                    set_hint(label, "Decryption needs the IV that was used to encrypt.", "warn")
                else:
                    set_hint(label, "Optional when encrypting.")
            else:
                set_hint(label, "")
            return
        path = Path(text)
        if not path.is_file():
            set_hint(label, f"{what} file not found.", "error")
            return
        size = path.stat().st_size
        if size == 16:
            set_hint(label, f"16 bytes ({what}, 128 bits)", "ok")
        elif size in (32, 33):
            set_hint(label, f"{size} bytes (looks like hex text; 16 raw bytes expected otherwise)")
        else:
            set_hint(label, f"{size} bytes - {what} is normally 16 bytes (32 hex characters).", "warn")

    # ------------------------------------------------- Command building -----

    def _crypto_args(self) -> tuple[list[str], list[str]]:
        errors: list[str] = []
        args = [self.op, "--mode", "ctr", "--sbox", self.sbox.currentText()]
        if self.force.isChecked():
            args.append("--force")

        key = self.key.text()
        if not key:
            errors.append("select a key file first.")
        else:
            if not Path(key).is_file():
                errors.append(f"key file not found: {key}")
            args += ["--key", key]

        iv = self.iv.text()
        if iv:
            if not Path(iv).is_file():
                errors.append(f"IV file not found: {iv}")
            args += ["--iv", iv]

        data = self.data_in.text()
        if not data:
            errors.append("select an input file or enter text.")
        elif self.data_in.kind() == "file":
            if not Path(data).is_file():
                errors.append(f"input file not found: {data}")
            args += ["--in", data]
        else:
            args += ["--text", data]

        if self.data_out.text():
            args += ["--out", self.data_out.text()]
        return args, errors

    def _copy_command(self) -> None:
        args, errors = self._crypto_args()
        if errors:
            self._warn("Cannot copy the command yet", errors)
            return
        QGuiApplication.clipboard().setText(format_command([self.tool_path.text().strip(), *args]))
        self.statusBar().showMessage("Command copied to clipboard", 4000)

    def _warn(self, title: str, errors: list[str]) -> None:
        QMessageBox.warning(self, title, "\n".join("• " + e[0].upper() + e[1:] for e in errors))

    # ------------------------------------------------------------- Actions --

    def run_crypto(self) -> None:
        if self._is_running():
            return
        args, errors = self._crypto_args()
        if errors:
            self._warn("Missing or invalid input", errors)
            return
        out = self.data_out.text()
        self._start(args, output=Path(out) if out else None)

    def _confirm_overwrite(self, path: Path) -> bool:
        if not path.exists():
            return True
        answer = QMessageBox.question(
            self, "Overwrite file?", f"{path.name} already exists. Overwrite it?"
        )
        return answer == QMessageBox.StandardButton.Yes

    def run_keygen(self) -> None:
        out = self.keygen_out.text()
        if not out:
            self._warn("Missing input", ["choose where to save the key."])
            return
        path = Path(out)
        if not self._confirm_overwrite(path):
            return

        def use_key() -> None:
            self.key.set_value("file", out)
            self._log("Key file filled in on the Encrypt / Decrypt tab.", "info")

        self._start(["keygen", "--out", out, "--force"], output=path, after=use_key)

    def run_ivgen(self) -> None:
        out = self.ivgen_out.text()
        if not out:
            self._warn("Missing input", ["choose where to save the IV."])
            return
        path = Path(out)
        if not self._confirm_overwrite(path):
            return

        def use_iv() -> None:
            self.iv.set_value("file", out)
            self._log("IV file filled in on the Encrypt / Decrypt tab.", "info")

        self._start(["ivgen", "--out", out, "--force"], output=path, after=use_iv)

    def run_kat(self) -> None:
        path = self.kat_path.text()
        if not path:
            self._warn("Missing input", ["choose a KAT vectors file (.json)."])
            return
        args = ["--kat", path]
        if self.kat_verbose.isChecked():
            args.append("--verbose")
        self._start(args)

    def run_xval(self) -> None:
        ref = self.xval_ref.text().strip() or "all"
        self._start(["xval", "--ref", ref, "--cases", str(self.xval_cases.value())])

    def run_bench(self) -> None:
        sizes = self.bench_sizes.text().strip()
        out = self.bench_out.text()
        errors = []
        if not sizes:
            errors.append("enter at least one size, e.g. 1MiB,100MiB.")
        if not out:
            errors.append("choose where to save the CSV file.")
        if errors:
            self._warn("Missing input", errors)
            return
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        self._start(
            ["bench", "--sizes", sizes, "--runs", str(self.bench_runs.value()), "--csv", out],
            output=Path(out),
        )

    # ------------------------------------------------------------- Process --

    def _is_running(self) -> bool:
        return self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning

    def _start(self, args: list[str], output: Path | None = None, after=None) -> None:
        if self._is_running():
            QMessageBox.warning(self, "Busy", "Another aestool command is still running.")
            return
        tool = Path(self.tool_path.text().strip())
        if not tool.is_file():
            QMessageBox.critical(
                self,
                "aestool not found",
                f"File not found:\n{tool}\n\nBuild aestool or choose its path in the bar at the top.",
            )
            return

        self._log(f"[{datetime.now():%H:%M:%S}] $ {format_command([str(tool), *args])}", "cmd")
        self._pending_output = output
        self._after = after
        self._cancelled = False

        self.process = QProcess(self)
        self.process.setProgram(str(tool))
        self.process.setArguments(args)
        self.process.setWorkingDirectory(str(LAB_ROOT))
        environment = QProcessEnvironment.systemEnvironment()
        environment.insert("PYTHONIOENCODING", "utf-8")
        self.process.setProcessEnvironment(environment)
        self.process.readyReadStandardOutput.connect(lambda: self._read(False))
        self.process.readyReadStandardError.connect(lambda: self._read(True))
        self.process.finished.connect(self._on_finished)
        self.process.errorOccurred.connect(self._on_error)
        self._set_busy(True)
        self.process.start()

    def _set_busy(self, busy: bool) -> None:
        for button in self.run_buttons:
            button.setEnabled(not busy)
        self.stop_btn.setEnabled(busy)
        self.progress.setVisible(busy)
        if busy:
            self.statusBar().showMessage("Running…")

    def _read(self, stderr: bool) -> None:
        if self.process is None:
            return
        data = self.process.readAllStandardError() if stderr else self.process.readAllStandardOutput()
        text = bytes(data).decode("utf-8", errors="replace").rstrip()
        if text:
            self._log(text, "err" if stderr else "out")

    def _on_finished(self, code: int, status: QProcess.ExitStatus) -> None:
        self._read(False)
        self._read(True)
        crashed = status == QProcess.ExitStatus.CrashExit
        self._set_busy(False)
        if self._cancelled:
            self._log("■ Stopped by user.", "err")
            self.statusBar().showMessage("Stopped", 6000)
        elif code == 0 and not crashed:
            self._log("✔ Done (exit code 0)", "ok")
            self.statusBar().showMessage("Done", 6000)
            if self._pending_output is not None and self._pending_output.exists():
                self.last_output = self._pending_output
                self.open_btn.setEnabled(True)
            if self._after is not None:
                self._after()
        else:
            reason = "process crashed" if crashed else f"exit code {code}"
            self._log(f"✖ Failed ({reason})", "err")
            self.statusBar().showMessage("Failed - see the log for details", 8000)
        self._after = None

    def _on_error(self, error: QProcess.ProcessError) -> None:
        if error == QProcess.ProcessError.FailedToStart:
            self._set_busy(False)
            self._log("✖ Could not start aestool. Check the path and execute permission.", "err")
            self.statusBar().showMessage("Could not start aestool", 8000)

    def _cancel(self) -> None:
        if not self._is_running():
            return
        self._cancelled = True
        self.process.terminate()
        QTimer.singleShot(1500, self._force_kill)

    def _force_kill(self) -> None:
        if self._is_running():
            self.process.kill()

    # ----------------------------------------------------------------- Log --

    def _log(self, text: str, kind: str = "out") -> None:
        self._log_entries.append((text, kind))
        if len(self._log_entries) > 5000:
            del self._log_entries[:1000]
        self._render_log_entry(text, kind)

    def _render_log_entry(self, text: str, kind: str) -> None:
        palette = THEMES[self.theme]
        color = {
            "cmd": palette["cmd"],
            "ok": palette["ok"],
            "err": palette["error"],
            "info": palette["muted"],
            "out": palette["log_text"],
        }[kind]
        body = html.escape(text).replace("\n", "<br>").replace("  ", "&nbsp; ")
        weight = "600" if kind in ("cmd", "ok") else "400"
        self.log.appendHtml(f'<span style="color:{color}; font-weight:{weight};">{body}</span>')

    def _rerender_log(self) -> None:
        self.log.clear()
        for text, kind in self._log_entries:
            self._render_log_entry(text, kind)

    def _clear_log(self) -> None:
        self._log_entries.clear()
        self.log.clear()

    def _save_log(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Save log", str(LAB_ROOT / "aestool_log.txt"), "Text (*.txt)"
        )
        if path:
            Path(path).write_text(self.log.toPlainText(), encoding="utf-8")
            self.statusBar().showMessage(f"Log saved: {path}", 5000)

    def _open_output_folder(self) -> None:
        if self.last_output is not None:
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.last_output.parent)))


def main() -> int:
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = AesToolWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())