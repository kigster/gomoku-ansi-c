"""Swappable logger facade backed by structlog + optional Logfire.

Usage throughout the codebase:
    from app.logger import get_logger
    log = get_logger("gomoku.auth")
    log.info("signup", username="alice")

Logging setup is triggered automatically by app/__init__.py.
"""

import logging
import os
import sys
from logging.handlers import RotatingFileHandler
from pathlib import Path

import structlog

LOG_DIR = Path(__file__).resolve().parent.parent / "logs"

_configured = False


def setup() -> None:
    """Configure structlog + stdlib logging + optional Logfire. Safe to call multiple times."""
    global _configured
    if _configured:
        return
    _configured = True

    LOG_DIR.mkdir(parents=True, exist_ok=True)

    environment = os.environ.get("ENVIRONMENT", "development")
    log_file = LOG_DIR / f"api.{environment}.log"

    # Configure Logfire only when a token is present and not in test
    logfire_processor = None
    if environment != "test" and os.environ.get("LOGFIRE_TOKEN"):
        try:
            import logfire

            logfire.configure()
            logfire_processor = logfire.StructlogProcessor()
        except Exception:
            pass

    # stdlib root logger — structlog renders, stdlib routes to handlers
    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    root.handlers.clear()

    # Console handler (stderr)
    console = logging.StreamHandler(sys.stderr)
    console.setLevel(logging.DEBUG)
    root.addHandler(console)

    # Rotating file handler
    file_handler = RotatingFileHandler(
        log_file, maxBytes=5 * 1024 * 1024, backupCount=5, encoding="utf-8"
    )
    file_handler.setLevel(logging.DEBUG)
    root.addHandler(file_handler)

    # Quiet noisy third-party loggers
    for name in ("uvicorn.access", "asyncpg", "httpcore", "httpx"):
        logging.getLogger(name).setLevel(logging.WARNING)

    shared_processors: list[structlog.types.Processor] = [
        structlog.contextvars.merge_contextvars,
        structlog.stdlib.add_log_level,
        structlog.stdlib.add_logger_name,
        structlog.processors.TimeStamper(fmt="%Y-%m-%d %H:%M:%S", utc=False),
        structlog.processors.StackInfoRenderer(),
        structlog.processors.format_exc_info,
        structlog.processors.UnicodeDecoder(),
    ]

    if logfire_processor:
        shared_processors.append(logfire_processor)

    structlog.configure(
        processors=[
            *shared_processors,
            structlog.stdlib.ProcessorFormatter.wrap_for_formatter,
        ],
        logger_factory=structlog.stdlib.LoggerFactory(),
        wrapper_class=structlog.stdlib.BoundLogger,
        cache_logger_on_first_use=True,
    )

    formatter = structlog.stdlib.ProcessorFormatter(
        processors=[
            structlog.stdlib.ProcessorFormatter.remove_processors_meta,
            structlog.dev.ConsoleRenderer(colors=sys.stderr.isatty()),
        ],
    )

    plain_formatter = structlog.stdlib.ProcessorFormatter(
        processors=[
            structlog.stdlib.ProcessorFormatter.remove_processors_meta,
            structlog.dev.ConsoleRenderer(colors=False),
        ],
    )

    console.setFormatter(formatter)
    file_handler.setFormatter(plain_formatter)


def get_logger(name: str | None = None) -> structlog.stdlib.BoundLogger:
    """Return a bound structlog logger."""
    return structlog.get_logger(name)
