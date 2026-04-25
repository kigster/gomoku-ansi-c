"""Tests for Settings.database_dsn property branches."""

from app.config import Settings


class TestDatabaseDsn:
    def test_returns_database_url_when_set(self):
        s = Settings(database_url="postgresql://u@h/db")
        assert s.database_dsn == "postgresql://u@h/db"

    def test_builds_dsn_with_socket(self):
        s = Settings(database_url="", db_socket="/cloudsql/project:region:instance")
        assert s.database_dsn == (
            "postgresql://postgres@/gomoku?host=/cloudsql/project:region:instance"
        )

    def test_builds_dsn_with_socket_and_password(self):
        s = Settings(database_url="", db_socket="/tmp", db_password="secret")
        assert s.database_dsn == "postgresql://postgres:secret@/gomoku?host=/tmp"

    def test_builds_dsn_localhost_fallback(self):
        s = Settings(database_url="", db_user="app", db_name="mydb")
        assert s.database_dsn == "postgresql://app@localhost/mydb"

    def test_builds_dsn_localhost_with_password(self):
        s = Settings(database_url="", db_user="app", db_password="pw", db_name="mydb")
        assert s.database_dsn == "postgresql://app:pw@localhost/mydb"
