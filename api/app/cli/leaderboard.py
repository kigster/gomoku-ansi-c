"""Leaderboard subcommands — refresh, show, stats."""

import asyncio
from typing import Annotated

import asyncpg
import typer
from rich import box
from rich.console import Console
from rich.table import Table

from app.config import settings

console = Console()

app = typer.Typer(
    help="Manage the [bold green]leaderboard[/bold green] materialized view.",
    rich_markup_mode="rich",
    no_args_is_help=True,
)


async def _refresh(concurrently: bool) -> int:
    conn = await asyncpg.connect(settings.database_dsn)
    try:
        # Check if the matview has been populated before
        is_populated = await conn.fetchval(
            "SELECT ispopulated FROM pg_matviews WHERE matviewname = 'leaderboard'"
        )
        if concurrently and is_populated:
            await conn.execute("REFRESH MATERIALIZED VIEW CONCURRENTLY leaderboard")
        else:
            await conn.execute("REFRESH MATERIALIZED VIEW leaderboard")
        count = await conn.fetchval("SELECT COUNT(*) FROM leaderboard")
        return count
    finally:
        await conn.close()


@app.command()
def refresh(
    concurrently: Annotated[
        bool,
        typer.Option(
            "--concurrently/--full",
            help="Use CONCURRENTLY (non-blocking) or full refresh.",
        ),
    ] = True,
):
    """Refresh the leaderboard materialized view.

    By default uses [bold]CONCURRENTLY[/bold] so reads are not blocked.
    Falls back to a full refresh when the view has no data yet.
    """
    with console.status("[bold cyan]Refreshing leaderboard…[/bold cyan]"):
        count = asyncio.run(_refresh(concurrently))
    console.print(f"[bold green]✓[/bold green] Leaderboard refreshed — {count} players ranked.")


async def _show(limit: int) -> list[dict]:
    conn = await asyncpg.connect(settings.database_dsn)
    try:
        rows = await conn.fetch(
            "SELECT leaderboard_number, username, max_score, avg_score, "
            "games_count, games_won, win_percentage, ai_depth_average, "
            "total_time_played_seconds "
            "FROM leaderboard ORDER BY leaderboard_number LIMIT $1",
            limit,
        )
        return [dict(r) for r in rows]
    finally:
        await conn.close()


@app.command()
def show(
    limit: Annotated[
        int,
        typer.Option("--limit", "-n", help="Number of rows to display."),
    ] = 25,
):
    """Display the current leaderboard rankings."""
    rows = asyncio.run(_show(limit))
    if not rows:
        console.print("[yellow]Leaderboard is empty. Run [bold]refresh[/bold] first.[/yellow]")
        raise typer.Exit()

    table = Table(
        title="Gomoku Leaderboard",
        box=box.ROUNDED,
        title_style="bold magenta",
        header_style="bold cyan",
        row_styles=["", "dim"],
    )
    table.add_column("#", justify="right", style="bold")
    table.add_column("Player", style="green")
    table.add_column("Best", justify="right", style="bold yellow")
    table.add_column("Avg", justify="right")
    table.add_column("Games", justify="right")
    table.add_column("Won", justify="right")
    table.add_column("Win%", justify="right")
    table.add_column("Depth", justify="right")
    table.add_column("Time(s)", justify="right")

    for r in rows:
        table.add_row(
            str(r["leaderboard_number"]),
            r["username"],
            str(r["max_score"]),
            str(r["avg_score"]),
            str(r["games_count"]),
            str(r["games_won"]),
            f"{r['win_percentage']}%",
            str(r["ai_depth_average"]),
            str(r["total_time_played_seconds"]),
        )

    console.print(table)


async def _stats() -> dict:
    conn = await asyncpg.connect(settings.database_dsn)
    try:
        row = await conn.fetchrow(
            "SELECT COUNT(*) AS players, "
            "COALESCE(MAX(max_score), 0) AS top_score, "
            "COALESCE(ROUND(AVG(max_score)), 0) AS avg_best, "
            "COALESCE(SUM(games_count), 0) AS total_games "
            "FROM leaderboard"
        )
        return dict(row)
    finally:
        await conn.close()


@app.command()
def stats():
    """Show summary statistics from the leaderboard."""
    data = asyncio.run(_stats())
    console.print()
    console.print("[bold magenta]Leaderboard Summary[/bold magenta]")
    console.print(f"  Players ranked:  [bold]{data['players']}[/bold]")
    console.print(f"  Total games:     [bold]{data['total_games']}[/bold]")
    console.print(f"  Top score:       [bold yellow]{data['top_score']}[/bold yellow]")
    console.print(f"  Average best:    [bold]{data['avg_best']}[/bold]")
    console.print()
