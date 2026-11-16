#!/bin/bash
# Notarize a signed DMG with an App Store Connect API key, staple the
# ticket and check that Gatekeeper accepts it.
#
# Usage: notarize_dmg.sh <dmg_path> <key_p8_path> <key_id> <issuer_id>

set -euo pipefail

if [ $# -ne 4 ]; then
    echo "Usage: notarize_dmg.sh <dmg_path> <key_p8_path> <key_id> <issuer_id>"
    exit 1
fi

DMG_PATH="$1"
KEY_PATH="$2"
KEY_ID="$3"
ISSUER_ID="$4"

AUTH=(--key "$KEY_PATH" --key-id "$KEY_ID" --issuer "$ISSUER_ID")
RESULT_JSON="$(mktemp)"
trap 'rm -f "$RESULT_JSON"' EXIT

echo "Submitting $DMG_PATH for notarization..."
set +e
xcrun notarytool submit "$DMG_PATH" "${AUTH[@]}" --wait --timeout 45m \
    --output-format json > "$RESULT_JSON"
submit_rc=$?
set -e

SUBMISSION_ID="$(plutil -extract id raw -o - "$RESULT_JSON" 2>/dev/null || true)"
STATUS="$(plutil -extract status raw -o - "$RESULT_JSON" 2>/dev/null || true)"
echo "notarytool submission id: ${SUBMISSION_ID:-unknown}"
echo "notarytool status: ${STATUS:-unknown}"

if [ -n "$SUBMISSION_ID" ]; then
    echo "----- notarization log -----"
    xcrun notarytool log "$SUBMISSION_ID" "${AUTH[@]}" || true
    echo "----------------------------"
fi

if [ $submit_rc -ne 0 ] || [ "$STATUS" != "Accepted" ]; then
    echo "Notarization failed"
    exit 1
fi

echo "Stapling ticket..."
xcrun stapler staple "$DMG_PATH"
xcrun stapler validate "$DMG_PATH"

echo "Gatekeeper check of the DMG:"
spctl --assess --type open --context context:primary-signature -vv "$DMG_PATH"

echo "Gatekeeper check of the app inside the DMG:"
MOUNT_DIR="$(mktemp -d)"
hdiutil attach -nobrowse -readonly -mountpoint "$MOUNT_DIR" "$DMG_PATH" > /dev/null
set +e
APP_IN_DMG="$(find "$MOUNT_DIR" -maxdepth 1 -name '*.app' | head -1)"
spctl --assess --type exec -vv "$APP_IN_DMG"
spctl_rc=$?
set -e
hdiutil detach "$MOUNT_DIR" > /dev/null || true
rmdir "$MOUNT_DIR" 2>/dev/null || true
exit $spctl_rc
