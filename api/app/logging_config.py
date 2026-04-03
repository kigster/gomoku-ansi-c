"""Colored console + rotating file logging for the Gomoku API."""

import logging
import sys
from logging.handlers import RotatingFileHandler
from pathlib import Path

LOG_DIR = Path(__file__).resolve().parent.parent / "logs"
LOG_FILE = LOG_DIR / "api.log"

# ANSI color codes
COLORS = {
    "DEBUG": "\033[36m",  # cyan
    "INFO": "\033[32m",  # green
    "WARNING": "\033[33m",  # yellow
    "ERROR": "\033[31m",  # red
    "CRITICAL": "\033[1;31m",  # bold red
}
RESET = "\033[0m"
DIM = "\033[2m"


class ColoredFormatter(logging.Formatter):
    """Console formatter with ANSI colors per log level."""

    def format(self, record: logging.LogRecord) -> str:
        color = COLORS.get(record.levelname, "")
        level = f"{color}{record.levelname:<8}{RESET}"
        timestamp = f"{DIM}{self.formatTime(record, '%H:%M:%S')}{RESET}"
        name = f"{DIM}{record.name}{RESET}"
        return f"{timestamp} {level} {name} — {record.getMessage()}"


class PlainFormatter(logging.Formatter):
    """File formatter — no ANSI codes, full timestamps."""

    def __init__(self) -> None:
        super().__init__(
            fmt="%(asctime)s %(levelname)-8s %(name)s — %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )


def setup_logging(level: str = "DEBUG") -> None:
    """Configure root logger with colored console + rotating file handlers."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    root = logging.getLogger()
    root.setLevel(getattr(logging, level.upper(), logging.DEBUG))

    # Remove any existing handlers (uvicorn adds its own)
    root.handlers.clear()

    # Console handler — colored
    console = logging.StreamHandler(sys.stderr)
    console.setFormatter(ColoredFormatter())
    console.setLevel(logging.DEBUG)
    root.addHandler(console)

    # File handler — rotating, 5 MB x 5 backups
    file_handler = RotatingFileHandler(
        LOG_FILE, maxBytes=5 * 1024 * 1024, backupCount=5, encoding="utf-8"
    )
    file_handler.setFormatter(PlainFormatter())
    file_handler.setLevel(logging.DEBUG)
    root.addHandler(file_handler)

    # Quiet down noisy third-party loggers
    logging.getLogger("uvicorn.access").setLevel(logging.WARNING)
    logging.getLogger("asyncpg").setLevel(logging.WARNING)
    logging.getLogger("httpcore").setLevel(logging.WARNING)
    logging.getLogger("httpx").setLevel(logging.INFO)
