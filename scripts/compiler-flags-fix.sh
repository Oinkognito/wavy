#!/bin/bash

BUILD_DIR="/build"
COMPILE_DB="$BUILD_DIR/compile_commands.json"
TEMP_DB="$BUILD_DIR/compile_commands.fixed.json"
FLAG="-mno-direct-extern-access"

echo "-- Removing $FLAG for Clang Tidy using jq..."

if [[ ! -f "$COMPILE_DB" ]]; then
  echo "[!] compile_commands.json not found at $COMPILE_DB"
  exit 1
fi

# Show where the flag was found
echo ">> Found occurrences of $FLAG in these entries:"
jq -r --arg flag "$FLAG" '
  map(select(.command | contains($flag)) | .file)
' "$COMPILE_DB"

# Remove the flag from command string
jq --arg flag "$FLAG" '
  map(
    .command |= gsub("(^| )" + $flag + "( |$)"; " ")
  )
' "$COMPILE_DB" > "$TEMP_DB" && mv "$TEMP_DB" "$COMPILE_DB"

echo "[x] Cleaned compile_commands.json by removing $FLAG"
