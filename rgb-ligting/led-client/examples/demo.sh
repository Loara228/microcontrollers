#!/usr/bin/env bash
set -euo pipefail

curl --fail-with-body -s \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "BASE 191, 64, 191 500" \
  http://127.0.0.1:5678/

sleep 2

curl --fail-with-body -s \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "FX 255 0 0 0 100 900" \
  http://127.0.0.1:5678/

sleep 2

curl --fail-with-body -s \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "FX 0 0 0 0 1000 0" \
  http://127.0.0.1:5678/

sleep 1

curl --fail-with-body -s \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "FX 255 255 255 255 1000 0" \
  http://127.0.0.1:5678/

sleep 1

curl --fail-with-body -s \
  -X POST \
  -H 'Content-Type: text/plain; charset=utf-8' \
  --data-binary "base 50 50 50 250" \
  http://127.0.0.1:5678/