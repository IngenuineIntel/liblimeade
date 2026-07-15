#!/usr/bin/env bash
set -euo pipefail

# Configure an OpenSSH subsystem entry for limeade protocol integrations.
# This script supports a dry-run mode by default; use --apply to modify files.

SSHD_CONFIG="/etc/ssh/sshd_config"
SSHD_DROPIN_DIR="/etc/ssh/sshd_config.d"
SNIPPET_NAME="99-limeade-subsystem.conf"
SNIPPET_PATH="${SSHD_DROPIN_DIR}/${SNIPPET_NAME}"
SUBSYSTEM_BIN="/usr/local/libexec/limeade-subsystem"
APPLY=0

usage() {
  cat <<'EOF'
Usage: configure_ssh_subsystem.sh [--apply] [--subsystem-bin PATH]

Options:
  --apply               Apply changes to /etc/ssh (requires root)
  --subsystem-bin PATH  Subsystem command path (default: /usr/local/libexec/limeade-subsystem)
  -h, --help            Show this help

Notes:
  - Without --apply, this script only prints intended changes.
  - The configured subsystem line is:
      Subsystem limeade <subsystem-bin>
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --apply)
      APPLY=1
      shift
      ;;
    --subsystem-bin)
      SUBSYSTEM_BIN="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

LINE="Subsystem limeade ${SUBSYSTEM_BIN}"

echo "Planned subsystem line:"
echo "  ${LINE}"

if [[ ${APPLY} -eq 0 ]]; then
  cat <<EOF

Dry run complete. To apply:
  sudo $(basename "$0") --apply --subsystem-bin "${SUBSYSTEM_BIN}"
EOF
  exit 0
fi

if [[ "${EUID}" -ne 0 ]]; then
  echo "--apply requires root privileges." >&2
  exit 1
fi

mkdir -p "${SSHD_DROPIN_DIR}"

cat > "${SNIPPET_PATH}" <<EOF
# Managed by configure_ssh_subsystem.sh
${LINE}
EOF

chmod 0644 "${SNIPPET_PATH}"

echo "Wrote ${SNIPPET_PATH}"

if command -v sshd >/dev/null 2>&1; then
  if sshd -t; then
    echo "sshd config test: OK"
  else
    echo "sshd config test: FAILED" >&2
    exit 1
  fi
fi

if command -v systemctl >/dev/null 2>&1; then
  if systemctl is-enabled ssh >/dev/null 2>&1 || systemctl is-active ssh >/dev/null 2>&1; then
    systemctl reload ssh || systemctl restart ssh
    echo "Reloaded ssh service."
  elif systemctl is-enabled sshd >/dev/null 2>&1 || systemctl is-active sshd >/dev/null 2>&1; then
    systemctl reload sshd || systemctl restart sshd
    echo "Reloaded sshd service."
  else
    echo "ssh/sshd service not found via systemctl; reload manually if needed."
  fi
else
  echo "systemctl not found; reload SSH daemon manually."
fi

echo "Limeade SSH subsystem configuration complete."
