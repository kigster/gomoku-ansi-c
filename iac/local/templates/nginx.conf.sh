#!/usr/bin/env bash
# Generate haproxy.cfg with a dynamic number of backend workers.
# Usage: haproxy.cfg.sh <num_workers>
# shellcheck disable=SC2155
set -euo pipefail

# Usage:
# bash nginx.conf.sh <backend_port> <LETSENCRYPT_certificate> <LETSENCRYPT_private_key>
# Example:
# bash nginx.conf.sh \
#   10000 \
#   _www \
#   /Users/stevejobs/.letsencrypt/live/gomoku.games/fullchain.pem \
#   /Users/stevejobs/.letsencrypt/live/gomoku.games/privkey.pem

export BACKEND_PORT="${1:-10000}"
export NGINX_USER="${2:-"${USER}"}"
export LETSENCRYPT_CERTIFICATE="${3:-${HOME}/.letsencrypt/live/dev.gomoku.games/fullchain.pem}"
export LETSENCRYPT_PRIVATE_KEY="${4:-${HOME}/.letsencrypt/live/dev.gomoku.games/privkey.pem}"

export SCRIPT_DIR="$(cd "$(dirname "$(dirname "${BASH_SOURCE[0]}")")" && pwd -P)"
export PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
export FRONTEND_DIR="${PROJECT_DIR}/frontend/dist"

# https://nginx.org/en/docs/events.html#:~:text=select%20%E2%80%94%20standard%20method.,%2C%20NetBSD%202.0%2C%20and%20macOS.
declare CONNECTION_METHOD

if [[ $OS == darwin ]]; then
  export CONNECTION_METHOD=kqueue
elif [[ $OS == linux ]]; then
  export CONNECTION_METHOD=epoll
fi

cat <<'HEADER'
# nginx.conf - Load Balancer Configuration
# This config runs on both LB boxes (AZ-a and AZ-b)
# nginx handles SSL termination, static assets, and routes dynamic requests to HAProxy
HEADER

echo "
user ${NGINX_USER} staff;
"
cat <<'HEADER'
worker_processes 6;
error_log /var/log/nginx/error.log warn;
pid /var/run/nginx.pid;

events {
    worker_connections 1024;
    multi_accept on;
HEADER

echo "    use ${CONNECTION_METHOD};   
}
"

cat <<'HEADER'

http {
    include mime.types;
    default_type application/octet-stream;

    # Logging format with timing information
    log_format main '$remote_addr - $remote_user [$time_local] "$request" '
                    '$status $body_bytes_sent "$http_referer" '
                    '"$http_user_agent" "$http_x_forwarded_for" '
                    'rt=$request_time uct="$upstream_connect_time" '
                    'uht="$upstream_header_time" urt="$upstream_response_time"';

    access_log /var/log/nginx/access.log main;

    # Performance tuning
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;

    # Gzip compression
    gzip on;
    gzip_vary on;
    gzip_proxied any;
    gzip_comp_level 6;
    gzip_types text/plain text/css text/xml application/json application/javascript 
               application/xml application/xml+rss text/javascript;

    # Rate limiting zone (optional, adjust as needed)
    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=10r/s;

    # Upstream to local HAProxy pool
    # HAProxy/Envoy listen on port 10000 with SO_REUSEPORT
    upstream gomoku_backend {
        server 127.0.0.1:10000;
        keepalive 32;
    }

    # =========================================================================
    # Gomoku UI Server (Dev Version)
    # =========================================================================
    server {
        listen 80;
        server_name dev.gomoku.games;
        
        # Redirect HTTP to HTTPS
        return 301 https://$server_name$request_uri;
    }

    server {
        listen 443 ssl;
        http2 on;
        server_name dev.gomoku.games;
HEADER

echo "
        # SSL Configuration
        ssl_certificate ${LETSENCRYPT_CERTIFICATE};
        ssl_certificate_key ${LETSENCRYPT_PRIVATE_KEY};
"

cat <<'MIDDLE'
        ssl_session_timeout 1d;
        ssl_session_cache shared:SSL:50m;
        ssl_session_tickets off;

        # Modern SSL configuration
        ssl_protocols TLSv1.2 TLSv1.3;
        ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
        ssl_prefer_server_ciphers off;

        # Security headers
        add_header X-Frame-Options "SAMEORIGIN" always;
        add_header X-Content-Type-Options "nosniff" always;
        add_header X-XSS-Protection "1; mode=block" always;

        # Health check endpoint (handled locally by nginx for LB health)
        location = /nginx-health {
            access_log off;
            return 200 "healthy\n";
            add_header Content-Type text/plain;
        }
MIDDLE

echo "
        # Static assets (if any)
        location / {
            root ${PROJECT_DIR}/frontend/dist/;
            expires 30d;
            add_header Cache-Control public;
        }
"
cat << 'FOOTER'

        # API routes - proxy to HAProxy
        location /health {
            proxy_pass http://gomoku_backend/health;
            proxy_http_version 1.1;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_connect_timeout 5s;
            proxy_read_timeout 10s;
        }

        location /gomoku/ {
            # Optional rate limiting
            limit_req zone=api_limit burst=20 nodelay;

            proxy_pass http://gomoku_backend;
            proxy_http_version 1.1;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_set_header Connection "";

            # Longer timeout for AI computation (up to 60s)
            proxy_connect_timeout 5s;
            proxy_send_timeout 60s;
            proxy_read_timeout 60s;

            # Buffer settings for JSON responses
            proxy_buffering on;
            proxy_buffer_size 4k;
            proxy_buffers 8 4k;
        }
    }
}
FOOTER
