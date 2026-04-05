"""Entry point for the Gomoku CLI.

Usage:
    uv run python -m app.cli.main --help
    uv run python -m app.cli.main leaderboard refresh
"""

import typer

from app.cli.leaderboard import app as leaderboard_app

app = typer.Typer(
    name="gomoku",
    help="Administrative CLI for the Gomoku API.",
    rich_markup_mode="rich",
    no_args_is_help=True,
    pretty_exceptions_enable=True,
)

app.add_typer(
    leaderboard_app,
    name="leaderboard",
    help="Manage the leaderboard materialized view.",
)
app.add_typer(
    leaderboard_app,
    name="lead",
    help="Alias for [bold]leaderboard[/bold].",
    hidden=True,
)


if __name__ == "__main__":
    app()
