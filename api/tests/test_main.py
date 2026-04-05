from pathlib import Path

from app.main import STATIC_DIR


def test_static_dir_points_to_api_public():
    assert STATIC_DIR == Path(__file__).resolve().parent.parent / "public"
    assert STATIC_DIR.is_dir()
