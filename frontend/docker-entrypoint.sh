#!/bin/sh
set -e

# Default to K8s Envoy gateway (plain HTTP, local cluster DNS)
export BACKEND_URL="${BACKEND_URL:-envoy-gateway.gomoku.svc.cluster.local:10000}"
export BACKEND_PROTO="${BACKEND_PROTO:-http}"
export BACKEND_HOST="${BACKEND_HOST:-${BACKEND_URL}}"

echo "Starting nginx with backend: ${BACKEND_PROTO}://${BACKEND_URL} (Host: ${BACKEND_HOST})"

# Substitute only our variables in the nginx config template
# (explicit var list so nginx's own $variables are not touched)
envsubst '${BACKEND_URL} ${BACKEND_PROTO} ${BACKEND_HOST}' \
    < /etc/nginx/conf.d/gomoku.conf.template \
    > /etc/nginx/conf.d/gomoku.conf

exec nginx -g 'daemon off;'
