#!/bin/bash

# Function to remove the Wavy license block
remove_license() {
    local file="$1"
    local tmpfile
    tmpfile=$(mktemp)

    # Check if license block is present
    if grep -q "Wavy Project" "$file"; then
        # Use awk to remove the block between the start and end of the license comment
        awk '
        BEGIN { skip=0 }
        /^\s*\/\*{80,}/ { skip=1; next }       # Match start of license block (e.g. /********...)
        skip && /\*\// { skip=0; next }        # Match end of license block (line with just '*/')
        skip == 0 { print }
        ' "$file" > "$tmpfile"

        mv "$tmpfile" "$file"
        echo "-- Removed license from $file"
    else
        echo "-- Skipping $file (no license block found)"
    fi
}

# Check if files are provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <source-files>"
    exit 1
fi

# Loop through files
for file in "$@"; do
    if [ -f "$file" ]; then
        remove_license "$file"
    else
        echo "Skipping: $file does not exist"
    fi
done
