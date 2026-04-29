# Stage 1: Build — install dependencies with uv
FROM python:3.13-slim AS builder
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv
WORKDIR /app
COPY pyproject.toml uv.lock* ./
RUN uv sync --frozen --no-dev 2>/dev/null || uv sync --no-dev
COPY . app/

# How to interrupt the build if ./public is empty?
# RUN bash -c "test -f public/index.html || { echo 'No files in ./public!'; exit 1; }"

# Stage 2: Runtime — slim image with just the venv
FROM python:3.13-slim
WORKDIR /app
COPY --from=builder /app /app
ENV PATH="/app/.venv/bin:$PATH"

# Cloud Run sets PORT; default to 8000 for local
ENV PORT=8000

EXPOSE 8000

# Builder did `COPY . app/`, so the FastAPI source lives at /app/app/app/main.py
# and supporting files (alembic.ini, db/, public/) are at /app/app/. Run uvicorn
# from /app/app so `app.main` resolves to /app/app/app/main.py.
WORKDIR /app/app

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:${PORT}/health')" || exit 1

# Single-worker async on purpose. Cloud Run gives each instance 1 vCPU
# and 512Mi memory; running 4 uvicorn workers OOMs (each loads asyncpg
# + httpx + telemetry independently) and starves the startup probe.
# FastAPI is async-native — concurrency comes from instance scaling
# (max_instance_request_concurrency = 80) and additional Cloud Run
# instances, not from cramming workers into one container.
CMD uvicorn app.main:app --host 0.0.0.0 --port ${PORT}
