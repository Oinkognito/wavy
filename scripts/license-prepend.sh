#!/bin/bash

YEAR=$(date +"%Y")

read -r -d '' LICENSE_COMMENT <<EOF
/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) ${YEAR} Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

EOF

contains_license() {
    head -n 10 "$1" | grep -q "Wavy Project - High-Fidelity Audio Streaming"
}

insert_license() {
    local file="$1"
    local tmpfile=$(mktemp)
    
    # check for shebang (#!)
    first_line=$(head -n 1 "$file")

    if [[ "$first_line" == "#!"* ]]; then
        {
            echo "$first_line"
            printf "%s\n\n" "$LICENSE_COMMENT"  # Use printf to preserve formatting
            tail -n +2 "$file"  # Skip first line and append the rest
        } > "$tmpfile"
    else
        {
            # Preserve `#pragma once` at the very top if it exists
            if grep -q "^#pragma once" "$file"; then
                echo "#pragma once"
                printf "%s\n\n" "$LICENSE_COMMENT"
                grep -v "^#pragma once" "$file"  # Remove old `#pragma once`
            else
                printf "%s\n\n" "$LICENSE_COMMENT"
                cat "$file"
            fi
        } > "$tmpfile"
    fi

    mv "$tmpfile" "$file"
    echo "-- Prepended license to $file"
}

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <source-files>"
    exit 1
fi

for file in "$@"; do
    if [ -f "$file" ]; then
        if contains_license "$file"; then
            echo "-- Skipping $file (already contains license)"
        else
            insert_license "$file"
        fi
    else
        echo "Skipping: $file does not exist"
    fi
done
