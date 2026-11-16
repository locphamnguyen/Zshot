#!/bin/bash
# Sign an app bundle with a Developer ID identity, inside-out, with the
# hardened runtime and a secure timestamp, so it can be notarized.
#
# Usage: sign_app.sh <app_path> <sign_identity> [keychain]
#
# Order matters: every nested Mach-O (dylibs, Qt plugins, framework
# binaries) is signed first, then each .framework bundle, then the app.
# `codesign --deep` is not used: Apple discourages it for signing and it
# hides which nested file fails.

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: sign_app.sh <app_path> <sign_identity> [keychain]"
    exit 1
fi

APP_PATH="$1"
IDENTITY="$2"
KEYCHAIN="${3:-}"

if [ ! -d "$APP_PATH" ]; then
    echo "App bundle not found: $APP_PATH"
    exit 1
fi

SIGN_ARGS=(--force --sign "$IDENTITY" --options runtime --timestamp)
if [ -n "$KEYCHAIN" ]; then
    SIGN_ARGS+=(--keychain "$KEYCHAIN")
fi

MAIN_EXE="$APP_PATH/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP_PATH/Contents/Info.plist")"

sign() {
    codesign "${SIGN_ARGS[@]}" "$1"
}

# 1. Every nested Mach-O file except the main executable, deepest path first.
count=0
while IFS= read -r f; do
    [ "$f" = "$MAIN_EXE" ] && continue
    if file -b "$f" | grep -q 'Mach-O'; then
        sign "$f"
        count=$((count + 1))
    fi
done < <(find "$APP_PATH/Contents" -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) \
         | awk -F/ '{ print NF "\t" $0 }' | sort -rn | cut -f2-)
echo "Signed $count nested Mach-O files"

# 2. Framework bundles (seals their resources and Info.plist).
fw_count=0
if [ -d "$APP_PATH/Contents/Frameworks" ]; then
    while IFS= read -r fw; do
        if ! sign "$fw"; then
            # Some framework copies lack a valid bundle layout; the binary
            # inside is already signed in step 1, which is enough.
            echo "warning: could not sign framework bundle $fw, keeping binary signature"
            continue
        fi
        fw_count=$((fw_count + 1))
    done < <(find "$APP_PATH/Contents/Frameworks" -maxdepth 1 -type d -name '*.framework')
fi
echo "Signed $fw_count framework bundles"

# 3. The app itself.
sign "$APP_PATH"

echo "Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
codesign --display --verbose=2 "$APP_PATH" 2>&1 | grep -E '^(Identifier|Format|TeamIdentifier|Runtime Version|Timestamp|flags)=' || true
