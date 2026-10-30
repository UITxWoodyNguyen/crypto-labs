"""PyQt6 GUI for the Lab 1 Crypto++ AES command-line tool.

Run from the lab1 directory:
    python3 gui/aestool_gui.py

Highlights:
  * One shared form for encrypt / decrypt with a segmented switch.
  * The form adapts to the selected mode and validates key / IV lengths live.
  * Every input has a source picker (File / Hex / Text) and accepts drag & drop.
  * The exact command line is previewed live and can be copied.
  * Colored log, background process with Stop, saved settings, light / dark theme.

Rendering note: text boxes, combo boxes, spin boxes and checkboxes use the native
Fusion style driven by a QPalette (robust). The stylesheet only styles cards,
labels, primary / segmented buttons, tabs and the log.
"""

from __future__ import annotations

import html
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
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
        "warn_text": "#7a4308",
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
        "warn_text": "#f3d9a8",
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
QLabel#banner {
    background: @warn_bg; color: @warn_text;
    border: 1px solid @warn; border-radius: 6px; padding: 8px 10px;
}

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
# AES mode metadata
# --------------------------------------------------------------------------- #


@dataclass(frozen=True)
class ModeInfo:
    desc: str
    key_bytes: tuple[int, ...] = (16, 24, 32)
    iv_bytes: tuple[int, ...] = ()  # empty = mode does not use an IV
    aead: bool = False


MODES: dict[str, ModeInfo] = {
    "ecb": ModeInfo("ECB encrypts blocks independently and uses no IV. It leaks "
                    "patterns, so use it only for KAT or demos."),
    "cbc": ModeInfo("CBC XORs each block with the previous ciphertext. Needs a random "
                    "16-byte IV.", iv_bytes=(16,)),
    "cfb": ModeInfo("CFB turns AES into a stream cipher. Needs a 16-byte IV.",
                    iv_bytes=(16,)),
    "ofb": ModeInfo("OFB generates a keystream independent of the plaintext. 16-byte IV, "
                    "never reuse it.", iv_bytes=(16,)),
    "ctr": ModeInfo("CTR uses a 16-byte counter block. Never reuse it with the same key.",
                    iv_bytes=(16,)),
    "xts": ModeInfo("XTS is meant for disk encryption: double-length key (32 or 64 bytes) "
                    "and a 16-byte tweak.", key_bytes=(32, 64), iv_bytes=(16,)),
    "ccm": ModeInfo("CCM is an AEAD mode: 7-13 byte nonce, 4-16 byte tag, optional AAD.",
                    iv_bytes=tuple(range(7, 14)), aead=True),
    "gcm": ModeInfo("GCM is an AEAD mode: 12-byte nonce recommended, 16-byte tag. Never "
                    "repeat a nonce.", iv_bytes=(12,), aead=True),
}


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #


def parse_hex(text: str) -> bytes | None:
    cleaned = re.sub(r"[\s:]", "", text)
    if re.fullmatch(r"(?:[0-9a-fA-F]{2})+", cleaned):
        return bytes.fromhex(cleaned)
    return None


def format_command(parts: list[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(parts)
    return shlex.join(parts)


def format_sizes(sizes: tuple[int, ...]) -> str:
    if len(sizes) > 2 and sizes == tuple(range(sizes[0], sizes[-1] + 1)):
        return f"{sizes[0]}-{sizes[-1]}"
    return ", ".join(str(s) for s in sizes)


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
    """Stack widgets vertically in a layout (e.g. a hint under a field).

    Returns a layout, not a container widget, so word-wrapped labels keep their
    height-for-width and are not clipped.
    """
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
    """Input with a source picker (File / Hex / Text) and a Browse button."""

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
        self.settings = QSettings("Lab1", "aestool_gui")
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

        self.setWindowTitle("Lab 1 - Crypto++ AES")
        self.resize(1120, 880)
        self.setMinimumSize(920, 660)
        self._build_ui()
        self._build_shortcuts()
        self._restore_settings()
        self._apply_theme()
        self._ready = True
        self._on_mode_changed()
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
        title = QLabel("AES with Crypto++")
        title.setObjectName("appTitle")
        subtitle = QLabel("GUI for aestool, Lab 1")
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

    def _build_crypto_tab(self) -> QWidget:
        page = QWidget()
        outer = QVBoxLayout(page)
        outer.setContentsMargins(0, 10, 0, 0)
        outer.setSpacing(10)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        body = QWidget()
        columns = QHBoxLayout(body)
        columns.setContentsMargins(0, 0, 8, 0)
        columns.setSpacing(12)
        left, right = QVBoxLayout(), QVBoxLayout()
        left.setSpacing(12)
        right.setSpacing(12)

        # --- Operation & mode
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

        self.mode = QComboBox()
        self.mode.addItems(list(MODES))
        self.mode_desc = make_hint()
        form.addRow("Mode", stack(self.mode, self.mode_desc))
        self.ecb_banner = QLabel(
            "ECB is not secure. aestool only runs it with --allow-ecb; "
            "use it for KAT or demos only."
        )
        self.ecb_banner.setObjectName("banner")
        self.ecb_banner.setWordWrap(True)
        form.addRow(self.ecb_banner)
        left.addWidget(card)

        # --- Key & IV
        card, form = make_card("Key and IV")
        self.key = SourceField(
            [("File", "file"), ("Hex", "hex")],
            {"file": "Path to key file", "hex": "Key as hex, e.g. 000102…"},
            title="Select key file",
        )
        self.key_hint = make_hint()
        self.iv = SourceField(
            [("File", "file"), ("Hex", "hex")],
            {"file": "Path to IV / nonce file", "hex": "IV or nonce as hex"},
            title="Select IV / nonce file",
        )
        self.iv_hint = make_hint()
        form.addRow("Key", stack(self.key, self.key_hint))
        form.addRow("IV / nonce", stack(self.iv, self.iv_hint))
        left.addWidget(card)

        # --- AEAD
        card, form = make_card("Authentication (CCM / GCM)")
        self.aead_card = card
        self.aad = SourceField(
            [("Text", "text"), ("File", "file")],
            {"text": "Additional authenticated data (optional)", "file": "Path to AAD file"},
            title="Select AAD file",
        )
        self.tag_hex = QLineEdit()
        self.tag_hex.setPlaceholderText("Tag as hex (used when decrypting)")
        self.tag_hint = make_hint()
        self.tag_len = QComboBox()
        self.tag_len.addItems(["4", "6", "8", "10", "12", "14", "16"])
        self.tag_len.setCurrentText("16")
        form.addRow("AAD", self.aad)
        form.addRow("Tag", stack(self.tag_hex, self.tag_hint))
        form.addRow("CCM tag length", self.tag_len)
        left.addWidget(card)
        left.addStretch(1)

        # --- Data
        card, form = make_card("Data")
        self.data_in = SourceField(
            [("File", "file"), ("Text", "text")],
            {"file": "File to process (drop a file here)", "text": "Type text directly"},
            title="Select input file",
        )
        self.data_out = SourceField(
            [("File", "save")], {"save": "Optional - where to save the result"},
            title="Save output",
        )
        self.meta = SourceField(
            [("File", "save")], {"save": "Optional - metadata JSON file"},
            title="Save metadata",
        )
        form.addRow("Input", self.data_in)
        form.addRow("Output", self.data_out)
        form.addRow("Metadata", self.meta)
        right.addWidget(card)

        # --- Options
        card, form = make_card("Options")
        self.aead = QCheckBox("--aead")
        self.aead.setToolTip("Turned on automatically for CCM and GCM")
        self.allow_ecb = QCheckBox("--allow-ecb")
        self.verbose = QCheckBox("--verbose")
        flags = QHBoxLayout()
        flags.setSpacing(18)
        for box in (self.aead, self.allow_ecb, self.verbose):
            flags.addWidget(box)
        flags.addStretch(1)
        form.addRow("Flags", flags)
        self.encoding = QComboBox()
        self.encoding.addItems(["hex", "base64", "raw"])
        form.addRow("Output encoding", self.encoding)
        self.nonce_db = QLineEdit(".aestool_nonces")
        self.nonce_db.setToolTip("Nonce database used to detect nonce reuse")
        form.addRow("Nonce database", self.nonce_db)
        right.addWidget(card)
        right.addStretch(1)

        columns.addLayout(left, 1)
        columns.addLayout(right, 1)
        scroll.setWidget(body)
        outer.addWidget(scroll, 1)
        outer.addWidget(self._build_action_bar())

        self.mode.currentIndexChanged.connect(self._on_mode_changed)
        self.enc_btn.toggled.connect(self._on_op_changed)
        for field in (self.key, self.iv, self.aad, self.data_in, self.data_out, self.meta):
            field.changed.connect(self._refresh)
        for edit in (self.tag_hex, self.nonce_db):
            edit.textChanged.connect(self._refresh)
        for combo in (self.tag_len, self.encoding):
            combo.currentIndexChanged.connect(self._refresh)
        for box in (self.aead, self.allow_ecb, self.verbose):
            box.toggled.connect(self._refresh)
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
        self.run_btn = QPushButton("Encrypt")
        self.run_btn.setProperty("role", "primary")
        self.run_btn.setToolTip("Run (Ctrl+Enter)")
        self.run_btn.clicked.connect(self.run_crypto)
        self.run_buttons.append(self.run_btn)
        row.addWidget(self.preview, 1)
        row.addWidget(copy_btn)
        row.addWidget(self.stop_btn)
        row.addWidget(self.run_btn)
        return frame

    def _build_tools_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 10, 8, 0)
        layout.setSpacing(12)

        # Key generation
        card, form = make_card("Generate key")
        self.keygen_bits = QComboBox()
        self.keygen_bits.addItems(["128", "192", "256", "512"])
        self.keygen_bits.setCurrentText("256")
        self.keygen_bits.setToolTip("512 bits is for XTS (two AES-256 keys)")
        self.keygen_out = SourceField(
            [("File", "save")], {"save": "Where to save the key"}, title="Save key"
        )
        self.hex_key = QCheckBox("Save as hex (--hex)")
        keygen_btn = QPushButton("Generate key")
        keygen_btn.setProperty("role", "primary")
        keygen_btn.clicked.connect(self.run_keygen)
        self.run_buttons.append(keygen_btn)
        form.addRow("Length (bits)", self.keygen_bits)
        form.addRow("Save to", self.keygen_out)
        form.addRow(self.hex_key)
        form.addRow(right_aligned(keygen_btn))
        layout.addWidget(card)

        # KAT
        card, form = make_card("Known answer tests (KAT)")
        self.kat_path = SourceField(
            [("File", "file")],
            {"file": "KAT vectors JSON, e.g. kat/….json"},
            title="Select KAT vectors",
            start_dir=LAB_ROOT / "kat",
            file_filter="JSON (*.json)",
        )
        kat_btn = QPushButton("Run KAT")
        kat_btn.setProperty("role", "primary")
        kat_btn.clicked.connect(self.run_kat)
        self.run_buttons.append(kat_btn)
        form.addRow("Vectors", self.kat_path)
        form.addRow(right_aligned(kat_btn))
        layout.addWidget(card)

        # Benchmark
        card, form = make_card("Benchmark")
        self.bench_runs = QSpinBox()
        self.bench_runs.setRange(1, 1000)
        self.bench_runs.setValue(30)
        self.bench_quick = QCheckBox("Quick run (--quick)")
        self.bench_quick.setChecked(True)
        default_csv = LAB_ROOT / "output" / "ubuntu" / "benchmarks" / "bench.csv"
        self.bench_out = SourceField(
            [("File", "save")],
            {"save": "Where to save the CSV results"},
            title="Save benchmark results",
            start_dir=default_csv.parent,
            file_filter="CSV (*.csv)",
        )
        self.bench_out.edit.setText(str(default_csv))
        bench_btn = QPushButton("Run benchmark")
        bench_btn.setProperty("role", "primary")
        bench_btn.clicked.connect(self.run_bench)
        self.run_buttons.append(bench_btn)
        form.addRow("Runs", self.bench_runs)
        form.addRow(self.bench_quick)
        form.addRow("Save to", self.bench_out)
        form.addRow(right_aligned(bench_btn))
        layout.addWidget(card)
        layout.addStretch(1)
        return page

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
        clear_log.clicked.connect(lambda: self.log.clear())
        for button in (self.open_btn, copy_log, save_log, clear_log):
            header.addWidget(button)
        layout.addLayout(header)

        self.log = QPlainTextEdit()
        self.log.setObjectName("log")
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(5000)
        self.log.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self.log.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        self.log.setPlaceholderText("aestool output and errors appear here.")
        layout.addWidget(self.log, 1)
        return panel

    def _build_shortcuts(self) -> None:
        for sequence, callback in (
            ("Ctrl+Return", self.run_crypto),
            ("Ctrl+Enter", self.run_crypto),
            ("Ctrl+L", lambda: self.log.clear()),
            ("Ctrl+.", self._cancel),
        ):
            QShortcut(QKeySequence(sequence), self).activated.connect(callback)

    # ------------------------------------------------------------ Settings --

    def _restore_settings(self) -> None:
        saved_tool = self.settings.value("tool_path", "", type=str)
        if saved_tool and Path(saved_tool).is_file():
            self.tool_path.setText(saved_tool)
        mode = self.settings.value("mode", "cbc", type=str)
        if mode in MODES:
            self.mode.setCurrentText(mode)
        encoding = self.settings.value("encoding", "hex", type=str)
        if encoding in ("hex", "base64", "raw"):
            self.encoding.setCurrentText(encoding)
        geometry = self.settings.value("geometry")
        if geometry:
            self.restoreGeometry(geometry)
        sizes = self.settings.value("splitter")
        if sizes:
            self.splitter.restoreState(sizes)

    def closeEvent(self, event) -> None:  # noqa: N802
        self.settings.setValue("tool_path", self.tool_path.text().strip())
        self.settings.setValue("theme", self.theme)
        self.settings.setValue("mode", self.mode.currentText())
        self.settings.setValue("encoding", self.encoding.currentText())
        self.settings.setValue("geometry", self.saveGeometry())
        self.settings.setValue("splitter", self.splitter.saveState())
        if self._is_running():
            self.process.kill()
        super().closeEvent(event)

    # --------------------------------------------------------------- Theme --

    def _apply_theme(self) -> None:
        app = QApplication.instance()
        app.setPalette(build_palette(self.theme))
        self.setStyleSheet(build_qss(self.theme))
        preview_palette = QApplication.palette()
        preview_palette.setColor(QPalette.ColorRole.Text, QColor(THEMES[self.theme]["cmd"]))
        self.preview.setPalette(preview_palette)
        self.theme_btn.setText("Dark theme" if self.theme == "light" else "Light theme")

    def _toggle_theme(self) -> None:
        self.theme = "dark" if self.theme == "light" else "light"
        self._apply_theme()

    # ----------------------------------------------------------- Tool path --

    def _default_tool(self) -> Path:
        candidates = [
            LAB_ROOT / "build_ubuntu_gcc" / "aestool",
            LAB_ROOT / "build_ubuntu_clang" / "aestool",
            LAB_ROOT / "build_windows_gcc" / "aestool.exe",
            LAB_ROOT / "build_windows_clang" / "aestool.exe",
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

    def _aead_enabled(self) -> bool:
        return MODES[self.mode.currentText()].aead or self.aead.isChecked()

    def _on_mode_changed(self) -> None:
        if not self._ready:
            return
        self.aead.setChecked(MODES[self.mode.currentText()].aead)
        self._refresh()

    def _on_op_changed(self) -> None:
        self.run_btn.setText("Encrypt" if self.op == "encrypt" else "Decrypt")
        self._refresh()

    def _refresh(self) -> None:
        if not self._ready:
            return
        mode = self.mode.currentText()
        info = MODES[mode]
        set_hint(self.mode_desc, info.desc)
        self.ecb_banner.setVisible(mode == "ecb")

        aead_on = self._aead_enabled()
        self.aead_card.setEnabled(aead_on)
        self.tag_hex.setEnabled(aead_on and self.op == "decrypt")
        self.tag_len.setEnabled(aead_on and mode == "ccm")

        self._update_key_hint(mode, info)
        self._update_iv_hint(mode, info)
        self._update_tag_hint(aead_on, mode)

        args, errors = self._crypto_args()
        if errors:
            self.preview.setText("Incomplete: " + errors[0])
        else:
            tool_name = Path(self.tool_path.text().strip()).name or "aestool"
            self.preview.setText(format_command([tool_name, *args]))
        self.preview.setCursorPosition(0)

    def _update_key_hint(self, mode: str, info: ModeInfo) -> None:
        text = self.key.text()
        if not text:
            set_hint(self.key_hint, "")
        elif self.key.kind() == "hex":
            data = parse_hex(text)
            if data is None:
                set_hint(self.key_hint, "Invalid hex: use 0-9, a-f and an even number of digits.", "error")
            elif len(data) in info.key_bytes:
                set_hint(self.key_hint, f"{len(data)} bytes ({len(data) * 8} bits)", "ok")
            else:
                set_hint(
                    self.key_hint,
                    f"{len(data)} bytes - {mode.upper()} expects {format_sizes(info.key_bytes)} bytes.",
                    "warn",
                )
        else:
            path = Path(text)
            if path.is_file():
                set_hint(self.key_hint, f"File, {path.stat().st_size} bytes")
            else:
                set_hint(self.key_hint, "Key file not found.", "error")

    def _update_iv_hint(self, mode: str, info: ModeInfo) -> None:
        text = self.iv.text()
        if not info.iv_bytes:
            if text:
                set_hint(self.iv_hint, "ECB uses no IV, so this value will not be sent.", "warn")
            else:
                set_hint(self.iv_hint, "ECB needs no IV.")
        elif not text:
            if self.op == "decrypt":
                set_hint(self.iv_hint, "Decryption usually needs the IV / nonce used to encrypt.", "warn")
            else:
                set_hint(self.iv_hint, "Optional when encrypting.")
        elif self.iv.kind() == "hex":
            data = parse_hex(text)
            if data is None:
                set_hint(self.iv_hint, "Invalid hex: use 0-9, a-f and an even number of digits.", "error")
            elif len(data) in info.iv_bytes:
                set_hint(self.iv_hint, f"{len(data)} bytes", "ok")
            else:
                set_hint(
                    self.iv_hint,
                    f"{len(data)} bytes - {mode.upper()} uses {format_sizes(info.iv_bytes)} bytes.",
                    "warn",
                )
        else:
            path = Path(text)
            if path.is_file():
                set_hint(self.iv_hint, f"File, {path.stat().st_size} bytes")
            else:
                set_hint(self.iv_hint, "IV / nonce file not found.", "error")

    def _update_tag_hint(self, aead_on: bool, mode: str) -> None:
        text = self.tag_hex.text().strip()
        if not (aead_on and self.op == "decrypt" and text):
            set_hint(self.tag_hint, "")
            return
        data = parse_hex(text)
        if data is None:
            set_hint(self.tag_hint, "Invalid tag hex.", "error")
        elif mode == "ccm" and len(data) != int(self.tag_len.currentText()):
            set_hint(
                self.tag_hint,
                f"Tag is {len(data)} bytes but CCM tag length is set to {self.tag_len.currentText()}.",
                "warn",
            )
        else:
            set_hint(self.tag_hint, f"{len(data)} bytes", "ok")

    # ------------------------------------------------- Command building -----

    def _crypto_args(self) -> tuple[list[str], list[str]]:
        errors: list[str] = []
        mode = self.mode.currentText()
        info = MODES[mode]
        args = [self.op, "--mode", mode]

        key = self.key.text()
        if not key:
            errors.append("choose a key file or enter a hex key.")
        elif self.key.kind() == "hex":
            if parse_hex(key) is None:
                errors.append("the hex key is invalid.")
            args += ["--key-hex", key]
        else:
            if not Path(key).is_file():
                errors.append(f"key file not found: {key}")
            args += ["--key", key]

        iv = self.iv.text()
        if iv and info.iv_bytes:  # ECB does not use an IV
            if self.iv.kind() == "hex" and parse_hex(iv) is None:
                errors.append("the IV / nonce hex is invalid.")
            elif self.iv.kind() == "file" and not Path(iv).is_file():
                errors.append(f"IV / nonce file not found: {iv}")
            args += ["--iv", iv]

        data = self.data_in.text()
        if not data:
            errors.append("choose an input file or enter text.")
        elif self.data_in.kind() == "file":
            if not Path(data).is_file():
                errors.append(f"input file not found: {data}")
            args += ["--in", data]
        else:
            args += ["--text", data]

        for field, flag in ((self.data_out, "--out"), (self.meta, "--meta")):
            if field.text():
                args += [flag, field.text()]

        if self._aead_enabled():
            aad = self.aad.text()
            if aad:
                args += ["--aad" if self.aad.kind() == "file" else "--aad-text", aad]
            tag = self.tag_hex.text().strip()
            if self.op == "decrypt" and tag:
                if parse_hex(tag) is None:
                    errors.append("the tag hex is invalid.")
                args += ["--tag-hex", tag]

        if self.nonce_db.text().strip():
            args += ["--nonce-db", self.nonce_db.text().strip()]
        if self.encoding.currentText() != "hex" or self.op == "decrypt":
            args += ["--encode", self.encoding.currentText()]
        if mode == "ccm":
            args += ["--tag-len", self.tag_len.currentText()]
        for box, flag in (
            (self.aead, "--aead"),
            (self.allow_ecb, "--allow-ecb"),
            (self.verbose, "--verbose"),
        ):
            if box.isChecked():
                args.append(flag)
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
        if self.mode.currentText() == "ecb" and not self.allow_ecb.isChecked():
            answer = QMessageBox.question(
                self,
                "ECB is not secure",
                "ECB leaks data patterns and aestool only runs it with --allow-ecb.\n"
                "Turn on --allow-ecb and continue?",
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
            self.allow_ecb.setChecked(True)
            args, errors = self._crypto_args()
        out = self.data_out.text()
        self._start(args, output=Path(out) if out else None)

    def run_keygen(self) -> None:
        out = self.keygen_out.text()
        if not out:
            self._warn("Missing input", ["choose where to save the key."])
            return
        path = Path(out)
        if path.exists():
            answer = QMessageBox.question(
                self, "Overwrite file?", f"{path.name} already exists. Overwrite it with a new key?"
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
        args = ["keygen", "--bits", self.keygen_bits.currentText(), "--out", out]
        if self.hex_key.isChecked():
            args.append("--hex")

        def use_key() -> None:
            self.key.set_value("file", out)
            self._log("Key file filled in on the Encrypt / Decrypt tab.", "info")

        self._start(args, output=path, after=use_key)

    def run_kat(self) -> None:
        path = self.kat_path.text()
        if not path:
            self._warn("Missing input", ["choose a KAT vectors file (.json)."])
            return
        self._start(["--kat", path])

    def run_bench(self) -> None:
        out = self.bench_out.text()
        if not out:
            self._warn("Missing input", ["choose where to save the CSV results."])
            return
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        args = ["bench", "--runs", str(self.bench_runs.value()), "--out", out]
        if self.bench_quick.isChecked():
            args.append("--quick")
        self._start(args, output=Path(out))

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