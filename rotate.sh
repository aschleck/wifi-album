#!/usr/bin/env bash

set -e

while true; do
  pick="$(find "$2" -type f | sort -R | tail -1)"
  echo "Showing ${pick}"
  "$(dirname $0)/upload.sh" "$1" "${pick}" || echo "failed to show ${pick}" # if it fails whatever
  sleep 300
done
