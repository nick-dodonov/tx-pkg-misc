#!/bin/bash
# Generate patch for Bazel module BUILD modifications
# Usage: ./generate_patch.sh <module-name>

set -e

if [ $# -eq 0 ]; then
    echo "Usage: $0 <module-name>"
    echo "Example: $0 boost.asio"
    exit 1
fi

MODULE_NAME="$1"
cd "$(dirname "$0")"

if [ ! -d "$MODULE_NAME" ]; then
    echo "Error: Module directory '$MODULE_NAME' not found"
    exit 1
fi

if [ ! -d "$MODULE_NAME/source" ] || [ ! -d "$MODULE_NAME/target" ]; then
    echo "Error: source/ or target/ directory not found in $MODULE_NAME/"
    exit 1
fi

echo "Generating patch for $MODULE_NAME from source/ to target/..."

# Clear existing patch file
> "$MODULE_NAME.patch"

# Generate diffs for all files that exist in both source and target
found_files=0
for source_file in "$MODULE_NAME/source"/*; do
    if [ -f "$source_file" ]; then
        filename=$(basename "$source_file")
        target_file="$MODULE_NAME/target/$filename"
        
        if [ -f "$target_file" ]; then
            found_files=$((found_files + 1))
            # Generate diff for this file
            diff -u "$source_file" "$target_file" | \
                sed "s|$MODULE_NAME/source/||g" | \
                sed "s|$MODULE_NAME/target/||g" >> "$MODULE_NAME.patch" || true
        fi
    fi
done

if [ $found_files -eq 0 ]; then
    echo "Error: No matching files found in source/ and target/"
    rm "$MODULE_NAME.patch"
    exit 1
fi

if [ -f "$MODULE_NAME.patch" ] && [ -s "$MODULE_NAME.patch" ]; then
    echo "✓ Patch generated: $MODULE_NAME.patch"
    echo ""
    echo "Files patched: $found_files"
    echo "Lines changed:"
    grep -E '^[+-]' "$MODULE_NAME.patch" | grep -v '^[+-][+-][+-]' | wc -l
else
    echo "✗ Failed to generate patch or no changes detected"
    rm -f "$MODULE_NAME.patch"
    exit 1
fi
