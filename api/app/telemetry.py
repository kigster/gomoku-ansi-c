"""OpenTelemetry → Honeycomb wiring.

Configures a TracerProvider and OTLP/HTTP exporter when HONEYCOMB_API_KEY is
set in the environment, then auto-instruments httpx and asyncpg. Call
``setup_telemetry()`` once at process start and ``instrument_app(app)`` after
the FastAPI app is created.

Honeycomb classic keys (32 chars) require ``x-honeycomb-dataset``; environment
keys (24 chars from new ingest) drive routing via the ``service.name`` resource
attribute alone. Both shapes are supported here.

Trace propagation: the auto-instrumented httpx client injects the W3C
``traceparent`` header into requests to gomoku-httpd, so the engine's logs can
be correlated with Honeycomb traces by trace_id.
"""

from __future__ import annotations

import os

from opentelemetry import trace
from opentelemetry.exporter.otlp.proto.http.trace_exporter import OTLPSpanExporter
from opentelemetry.instrumentation.asyncpg import AsyncPGInstrumentor
from opentelemetry.instrumentation.fastapi import FastAPIInstrumentor
from opentelemetry.instrumentation.httpx import HTTPXClientInstrumentor
from opentelemetry.sdk.resources import Resource
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor

from app.logger import get_logger

log = get_logger("gomoku.telemetry")

DEFAULT_OTLP_ENDPOINT = "https://api.honeycomb.io/v1/traces"

_initialized = False


def setup_telemetry(service_name: str = "gomoku-api") -> None:
    """Install a TracerProvider with an OTLP→Honeycomb exporter.

    No-op (with a warning log) when HONEYCOMB_API_KEY is missing — keeps local
    dev runs untraced without forcing developers to set up an exporter.
    """
    global _initialized
    if _initialized:
        return

    api_key = os.getenv("HONEYCOMB_API_KEY")
    if not api_key:
        log.info("telemetry_disabled", reason="no_honeycomb_api_key")
        return

    endpoint = os.getenv("OTEL_EXPORTER_OTLP_ENDPOINT", DEFAULT_OTLP_ENDPOINT)
    resolved_service = os.getenv("OTEL_SERVICE_NAME", service_name)
    dataset = os.getenv("HONEYCOMB_DATASET")

    # Honeycomb classic keys are 32 chars and require x-honeycomb-dataset.
    # New environment keys (24 chars) route by service.name via resource attrs.
    headers: dict[str, str] = {"x-honeycomb-team": api_key}
    if dataset or len(api_key) == 32:
        headers["x-honeycomb-dataset"] = dataset or resolved_service

    resource = Resource.create(
        {
            "service.name": resolved_service,
            # OTel-standard attribute, queryable in Honeycomb as `deployment.environment`.
            # Lets one Honeycomb env hold dev/test/prod traces; promote to separate
            # Honeycomb environments when traffic justifies it.
            "deployment.environment": os.getenv("ENVIRONMENT", "development"),
        }
    )
    provider = TracerProvider(resource=resource)
    provider.add_span_processor(
        BatchSpanProcessor(OTLPSpanExporter(endpoint=endpoint, headers=headers))
    )
    trace.set_tracer_provider(provider)

    HTTPXClientInstrumentor().instrument()
    AsyncPGInstrumentor().instrument()

    _initialized = True
    log.info(
        "telemetry_enabled",
        service=resolved_service,
        endpoint=endpoint,
        dataset=headers.get("x-honeycomb-dataset", "(env-routed)"),
    )


def instrument_app(fastapi_app) -> None:
    """Attach the FastAPI ASGI middleware that emits server spans."""
    if not _initialized:
        return
    FastAPIInstrumentor.instrument_app(fastapi_app)
