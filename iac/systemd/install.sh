#!/usr/bin/env bash
# install.sh - Install and configure gomoku-httpd systemd units
#
# Usage:
#   sudo ./install.sh [OPTIONS]
#
# Options:
#   -n, --num-instances N    Number of instances to create (default: CPU cores)
#   -p, --start-port PORT    Starting port number (default: 8787)
#   -d, --install-dir DIR    Installation directory (default: /opt/gomoku)
#   -u, --user USER          User to run as (default: gomoku)
#   -h, --help               Show this help message
#
# Examples:
#   sudo ./install.sh                    # Auto-detect CPU cores, start at 8787
#   sudo ./install.sh -n 5 -p 9000       # 5 instances starting at port 9000
#   sudo ./install.sh --num-instances 10 # 10 instances

set -euo pipefail

# Default configuration
NUM_INSTANCES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
START_PORT=8787
INSTALL_DIR="/opt/gomoku"
GOMOKU_USER="gomoku"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

usage() {
    head -20 "$0" | tail -18 | sed 's/^# //' | sed 's/^#//'
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-instances)
            NUM_INSTANCES="$2"
            shift 2
            ;;
        -p|--start-port)
            START_PORT="$2"
            shift 2
            ;;
        -d|--install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        -u|--user)
            GOMOKU_USER="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    log_error "This script must be run as root (use sudo)"
    exit 1
fi

log_info "Installing gomoku-httpd systemd units"
log_info "  Instances: $NUM_INSTANCES"
log_info "  Start port: $START_PORT"
log_info "  Install dir: $INSTALL_DIR"
log_info "  User: $GOMOKU_USER"

# Create gomoku user if it doesn't exist
if ! id "$GOMOKU_USER" &>/dev/null; then
    log_info "Creating user '$GOMOKU_USER'"
    useradd --system --no-create-home --shell /usr/sbin/nologin "$GOMOKU_USER"
fi

# Create directories
log_info "Creating directories"
mkdir -p "$INSTALL_DIR/bin"
mkdir -p /var/log/gomoku
chown "$GOMOKU_USER:$GOMOKU_USER" /var/log/gomoku

# Copy binary if it exists in the script directory or parent
BINARY_SRC=""
for path in "$SCRIPT_DIR/../../gomoku-httpd" "$SCRIPT_DIR/../gomoku-httpd" "./gomoku-httpd"; do
    if [[ -f "$path" ]]; then
        BINARY_SRC="$path"
        break
    fi
done

if [[ -n "$BINARY_SRC" ]]; then
    log_info "Copying gomoku-httpd binary from $BINARY_SRC"
    cp "$BINARY_SRC" "$INSTALL_DIR/bin/gomoku-httpd"
    chmod 755 "$INSTALL_DIR/bin/gomoku-httpd"
else
    log_warn "gomoku-httpd binary not found. Please copy it to $INSTALL_DIR/bin/gomoku-httpd"
fi

# Install systemd unit files
log_info "Installing systemd unit files"
cp "$SCRIPT_DIR/gomoku-httpd@.service" /etc/systemd/system/
cp "$SCRIPT_DIR/gomoku-httpd.target" /etc/systemd/system/

# Update install directory in service file if different from default
if [[ "$INSTALL_DIR" != "/opt/gomoku" ]]; then
    log_info "Updating install directory in service file"
    sed -i "s|/opt/gomoku|$INSTALL_DIR|g" /etc/systemd/system/gomoku-httpd@.service
fi

# Update user in service file if different from default
if [[ "$GOMOKU_USER" != "gomoku" ]]; then
    log_info "Updating user in service file"
    sed -i "s|User=gomoku|User=$GOMOKU_USER|g" /etc/systemd/system/gomoku-httpd@.service
    sed -i "s|Group=gomoku|Group=$GOMOKU_USER|g" /etc/systemd/system/gomoku-httpd@.service
fi

# Reload systemd
log_info "Reloading systemd daemon"
systemctl daemon-reload

# Enable instances
log_info "Enabling $NUM_INSTANCES instances starting at port $START_PORT"
END_PORT=$((START_PORT + NUM_INSTANCES - 1))
for port in $(seq "$START_PORT" "$END_PORT"); do
    systemctl enable "gomoku-httpd@$port.service" --quiet
done

# Enable target
systemctl enable gomoku-httpd.target --quiet

log_info "Installation complete!"
echo ""
echo "Next steps:"
echo "  1. Ensure gomoku-httpd binary is at: $INSTALL_DIR/bin/gomoku-httpd"
echo "  2. Start all instances:  sudo systemctl start gomoku-httpd.target"
echo "  3. Check status:         sudo systemctl status gomoku-httpd.target"
echo "  4. View logs:            journalctl -u 'gomoku-httpd@*' -f"
echo ""
echo "Individual instance management:"
echo "  Start one:    sudo systemctl start gomoku-httpd@$START_PORT"
echo "  Stop one:     sudo systemctl stop gomoku-httpd@$START_PORT"
echo "  Restart one:  sudo systemctl restart gomoku-httpd@$START_PORT"
echo ""
echo "Ports configured:"
for port in $(seq "$START_PORT" "$END_PORT"); do
    agent_port=$((port + 1000))
    echo "  HTTP: $port, Agent: $agent_port"
done
