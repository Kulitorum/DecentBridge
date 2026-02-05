#!/bin/bash
# Push skin files to a running DecentBridge instance
#
# Usage:
#   ./tools/push-skin.sh [host] [file...]
#
# Examples:
#   ./tools/push-skin.sh 192.168.1.208 index.html          # single file
#   ./tools/push-skin.sh 192.168.1.208 src/modules/ui.js    # nested file
#   ./tools/push-skin.sh 192.168.1.208                      # all changed files (git)
#
# Run from the streamline_project directory.

HOST="${1:-192.168.1.208}"
PORT=8080
shift

push_file() {
    local file="$1"
    echo "  $file ($(wc -c < "$file") bytes)"
    curl -s -X PUT --data-binary "@$file" "http://$HOST:$PORT/api/v1/dev/skin/$file" > /dev/null
}

if [ $# -gt 0 ]; then
    # Push specified files
    for file in "$@"; do
        push_file "$file"
    done
else
    # Push all modified files (git diff)
    echo "Pushing modified files to $HOST..."
    for file in $(git diff --name-only HEAD); do
        push_file "$file"
    done
    # Also push untracked files
    for file in $(git ls-files --others --exclude-standard); do
        push_file "$file"
    done
fi

echo "Done."
