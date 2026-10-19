#!/bin/sh
set -eu
printf '%s\n' 'Local Lab 2 validation; NOT the official 2026 grading script.'
exec python3 "$(dirname "$0")/test_lab2.py" "$@"
