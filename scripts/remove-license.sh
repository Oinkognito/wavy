#!/bin/bash

# Function to remove the license header
remove_license() {
    local file="$1"
    local tmpfile=$(mktemp)

    # Check if the file contains the Wavy project comment
    if grep -q "Wavy Project - High-Fidelity Audio Streaming" "$file"; then
        # Use sed to remove everything from the first /** to the closing ***/
        sed '1,/^\* \*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*/d' "$file" > "$tmpfile"

        # Replace the original file
        mv "$tmpfile" "$file"
        echo "-- Removed license from $file"
    else
        echo "-- Skipping $file (no license found)"
    fi
}

# Check if files are provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <source-files>"
    exit 1
fi

# Loop through all provided files
for file in "$@"; do
    if [ -f "$file" ]; then
        remove_license "$file"
    else
        echo "Skipping: $file does not exist"
    fi
done
