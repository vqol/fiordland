#!/usr/bin/env bash
set -euo pipefail
state=$(systemctl is-enabled bluetooth.service 2>/dev/null || true)
[ "$state" = disabled ] || exit 0
systemctl enable bluetooth.service
if [ -d /run/systemd/system ]; then
  systemctl start bluetooth.service || true
fi
