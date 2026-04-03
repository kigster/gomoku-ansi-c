import logging

from app.config import settings

logger = logging.getLogger(__name__)


async def send_password_reset_email(to_email: str, token: str) -> None:
    reset_url = f"https://gomoku.games/reset-password?token={token}"

    if settings.email_provider == "stdout":
        logger.info(f"[DEV] Password reset email to {to_email}: {reset_url}")
        print(f"\n{'=' * 60}")
        print("PASSWORD RESET EMAIL")
        print(f"To: {to_email}")
        print(f"Reset URL: {reset_url}")
        print(f"{'=' * 60}\n")
        return

    if settings.email_provider == "sendgrid":
        import httpx

        async with httpx.AsyncClient() as client:
            await client.post(
                "https://api.sendgrid.com/v3/mail/send",
                headers={
                    "Authorization": f"Bearer {settings.sendgrid_api_key}",
                    "Content-Type": "application/json",
                },
                json={
                    "personalizations": [{"to": [{"email": to_email}]}],
                    "from": {"email": settings.email_from, "name": "Gomoku"},
                    "subject": "Reset your Gomoku password",
                    "content": [
                        {
                            "type": "text/plain",
                            "value": (
                                f"Click this link to reset your password:\n\n{reset_url}\n\n"
                                "This link expires in 1 hour."
                            ),
                        }
                    ],
                },
            )
