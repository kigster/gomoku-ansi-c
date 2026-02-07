#!/usr/bin/env bash
# Install (creates N instances based on CPU cores)
sudo ./iac/systemd/install.sh

# Or customize
sudo ./iac/systemd/install.sh --num-instances 5 --start-port 8787

# Copy binary
sudo cp gomoku-httpd /opt/gomoku/bin/

# Start all
sudo systemctl start gomoku-httpd.target

# Check status
./iac/systemd/gomoku-httpd-ctl status
