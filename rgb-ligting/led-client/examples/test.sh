#!/usr/bin/env bash
set -euo pipefail

command="${1:-BASE 255 255 255 1500}"

curl --fail-with-body -i \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "$command" \
  http://127.0.0.1:5678/
