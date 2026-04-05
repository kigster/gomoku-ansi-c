from dotenv import load_dotenv

load_dotenv()

from app.logger import setup  # noqa: E402

setup()
