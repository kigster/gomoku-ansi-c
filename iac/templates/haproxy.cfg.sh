#!/usr/bin/env bash
# Generate haproxy.cfg with a dynamic number of backend workers.
# Usage: haproxy.cfg.sh <num_workers>
set -euo pipefail

WORKERS="${1:-10}"
BASE_PORT=9500
AGENT_BASE_PORT=9600

cat <<'HEADER'
# haproxy.cfg - Auto-generated from iac/templates/haproxy.cfg.sh
# Primary backend: Gomoku cluster (local development)
#
# Start with: haproxy -f haproxy.cfg -D

global
    log stdout format raw local0
    maxconn 4096
    stats socket /var/run/haproxy.sock mode 660 level admin
    daemon

defaults
    log global
    mode http
    option httplog
    option dontlognull
    option http-server-close
    option forwardfor except 127.0.0.0/8
    option redispatch
    retries 3
    timeout connect 5s
    timeout client 30s
    timeout server 30s
    timeout queue 600s
    default-server inter 2s fall 2 rise 1

# =========================================================================
# Frontend - receives requests from nginx on localhost
# =========================================================================
frontend gomoku_frontend
    bind 127.0.0.1:10000
    default_backend gomoku_primary

# =========================================================================
# Primary Backend - Gomoku cluster
# Uses agent-check for accurate busy/ready status
# =========================================================================
backend gomoku_primary
    balance leastconn
    timeout server 600000ms

HEADER

for ((i = 0; i < WORKERS; i++)); do
	port=$((BASE_PORT + i))
	agent_port=$((AGENT_BASE_PORT + i))
	printf '    server gomoku-%d 127.0.0.1:%d check agent-check agent-port %d agent-inter 1s weight 100\n' \
		"$((i + 1))" "$port" "$agent_port"
done

cat <<'FOOTER'

# =========================================================================
# Statistics page (optional, for monitoring)
# =========================================================================
frontend stats
    bind 127.0.0.1:8404
    mode http
    stats enable
    stats uri /stats
    stats refresh 1s
    stats admin if LOCALHOST
FOOTER
