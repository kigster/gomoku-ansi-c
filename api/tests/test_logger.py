"""Tests for logger setup and logfire conditional initialization."""

import logging
from unittest import mock

import pytest

import app.logger as logger_mod


@pytest.fixture
def reset_logger():
    """Reset the logger module so setup() can be called again."""
    old = logger_mod._configured
    yield logger_mod
    # Restore — let the app's own setup remain authoritative after the test
    logger_mod._configured = old


class TestLoggerSetup:
    def test_setup_creates_log_directory(self, reset_logger, tmp_path, monkeypatch):
        reset_logger._configured = False
        log_dir = tmp_path / "logs"
        monkeypatch.setattr(logger_mod, "LOG_DIR", log_dir)
        monkeypatch.setenv("ENVIRONMENT", "test")

        reset_logger.setup()
        assert log_dir.is_dir()

    def test_setup_is_idempotent(self, reset_logger, monkeypatch):
        reset_logger._configured = False
        monkeypatch.setenv("ENVIRONMENT", "test")
        reset_logger.setup()
        first_handlers = len(logging.getLogger().handlers)
        # Call again — should be a no-op
        reset_logger.setup()
        assert len(logging.getLogger().handlers) == first_handlers

    def test_get_logger_returns_usable_logger(self, reset_logger, monkeypatch):
        reset_logger._configured = False
        monkeypatch.setenv("ENVIRONMENT", "test")
        reset_logger.setup()
        log = logger_mod.get_logger("test.module")
        # structlog returns a BoundLoggerLazyProxy that wraps BoundLogger
        assert hasattr(log, "info")
        assert hasattr(log, "warning")
        assert hasattr(log, "error")

    def test_file_handler_writes_to_log(self, reset_logger, tmp_path, monkeypatch):
        reset_logger._configured = False
        log_dir = tmp_path / "logs"
        monkeypatch.setattr(logger_mod, "LOG_DIR", log_dir)
        monkeypatch.setenv("ENVIRONMENT", "test")

        reset_logger.setup()
        log = logger_mod.get_logger("test.file")
        log.info("hello from test")

        log_file = log_dir / "api.test.log"
        assert log_file.exists()
        content = log_file.read_text()
        assert "hello from test" in content


class TestLogfireConditional:
    def test_logfire_skipped_without_token(self, reset_logger, monkeypatch):
        """Logfire is NOT configured when LOGFIRE_TOKEN is absent."""
        reset_logger._configured = False
        monkeypatch.delenv("LOGFIRE_TOKEN", raising=False)
        monkeypatch.setenv("ENVIRONMENT", "development")

        with mock.patch("logfire.configure") as mock_configure:
            reset_logger.setup()
            mock_configure.assert_not_called()

    def test_logfire_skipped_in_test_env(self, reset_logger, monkeypatch):
        """Logfire is NOT configured in test environment even with token."""
        reset_logger._configured = False
        monkeypatch.setenv("LOGFIRE_TOKEN", "some-token")
        monkeypatch.setenv("ENVIRONMENT", "test")

        with mock.patch("logfire.configure") as mock_configure:
            reset_logger.setup()
            mock_configure.assert_not_called()

    def test_logfire_configured_with_token(self, reset_logger, monkeypatch):
        """Logfire IS configured when LOGFIRE_TOKEN is set and not in test."""
        reset_logger._configured = False
        monkeypatch.setenv("LOGFIRE_TOKEN", "test-token-value")
        monkeypatch.setenv("ENVIRONMENT", "development")

        with (
            mock.patch("logfire.configure") as mock_configure,
            mock.patch("logfire.StructlogProcessor"),
        ):
            reset_logger.setup()
            mock_configure.assert_called_once()
