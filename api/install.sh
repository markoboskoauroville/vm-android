#!/bin/bash
# Installs pixel7d (the test door, Tailscale :8897) and pixel7-jobs (the GitHub mailbox) as systemd services.
# Run on chromium-vm from the repo copy in ~/vm-android: sudo ./api/install.sh
set -euo pipefail
U=${SUDO_USER:-ubuntu}; H=$(getent passwd "$U" | cut -d: -f6); D="$H/vm-android"
install -d -m 700 -o "$U" -g "$U" "$H/.config/pixel7"
[ -s "$H/.config/pixel7/token" ] || { python3 -c 'import secrets;print(secrets.token_urlsafe(32))' > "$H/.config/pixel7/token"; chown "$U:$U" "$H/.config/pixel7/token"; chmod 600 "$H/.config/pixel7/token"; }
cat > /etc/systemd/system/pixel7d.service <<S
[Unit]
Description=pixel7d, the Pixel 7 test door (Tailscale only)
After=docker.service tailscaled.service network-online.target
Wants=network-online.target
[Service]
User=$U
ExecStartPre=/bin/sh -c 'for i in \$(seq 1 60); do ip -4 addr show tailscale0 | grep -q 100. && exit 0; sleep 2; done; exit 1'
ExecStart=/usr/bin/python3 $D/api/pixel7d.py
Restart=always
RestartSec=5
[Install]
WantedBy=multi-user.target
S
cat > /etc/systemd/system/pixel7-jobs.service <<S
[Unit]
Description=pixel7-jobs, Pixel 7 tests from the GitHub mailbox vm-android-jobs
After=docker.service network-online.target
Wants=network-online.target
[Service]
User=$U
ExecStart=/usr/bin/python3 $D/api/jobrunner.py
Restart=always
RestartSec=10
[Install]
WantedBy=multi-user.target
S
systemctl daemon-reload
systemctl enable --now pixel7d.service
[ -d "$H/vm-android-jobs/.git" ] && systemctl enable --now pixel7-jobs.service || echo "pixel7-jobs not started: clone vm-android-jobs to $H/vm-android-jobs first"
systemctl --no-pager --lines=0 status pixel7d.service | head -3
