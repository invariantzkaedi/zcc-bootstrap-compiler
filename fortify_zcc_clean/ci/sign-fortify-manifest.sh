#!/usr/bin/env bash
# Sign the fortify subject manifest with the configured signer.
# Always writes a signing-status JSON file describing the outcome.
#
# Inputs (env):
#   MANIFEST                 path to subject manifest (required)
#   FORTIFY_SIGNER           "" | minisign | gpg | cosign
#   FORTIFY_SIGNING_STATUS   path for the status JSON (default: artifacts/fortify-signing-status.json)
#   FORTIFY_SECRET_KEY       minisign secret key path (minisign only)
#   FORTIFY_GPG_KEY          gpg local-user key id (gpg only)
set -euo pipefail

MANIFEST="${MANIFEST:-artifacts/fortify-layout.subject.json}"
SIGNER="${FORTIFY_SIGNER:-}"
STATUS="${FORTIFY_SIGNING_STATUS:-artifacts/fortify-signing-status.json}"

if [ ! -f "$MANIFEST" ]; then
    echo "[FORTIFY-SIGN] manifest missing: $MANIFEST" >&2
    exit 1
fi

mkdir -p "$(dirname "$STATUS")"

# Emit a signing-status JSON file. Empty signature/certificate become null.
write_status() {
    local status="$1"
    local signer="$2"
    local sig="$3"
    local cert="$4"
    local required="$5"   # "true" | "false"

    local sig_json="null"
    local cert_json="null"
    [ -n "$sig" ]  && sig_json="\"$sig\""
    [ -n "$cert" ] && cert_json="\"$cert\""

    cat > "$STATUS" <<EOF
{
  "kind": "fortify-signing-status",
  "status": "$status",
  "signer": "$signer",
  "subject": "$MANIFEST",
  "required": $required,
  "signature": $sig_json,
  "certificate": $cert_json
}
EOF
}

case "$SIGNER" in
    "")
        write_status "skipped" "" "" "" "false"
        echo "[FORTIFY-SIGN] FORTIFY_SIGNER unset; status=skipped"
        ;;

    minisign)
        if ! command -v minisign >/dev/null 2>&1; then
            echo "[FORTIFY-SIGN] minisign not on PATH" >&2
            exit 1
        fi
        if [ -z "${FORTIFY_SECRET_KEY:-}" ]; then
            echo "[FORTIFY-SIGN] FORTIFY_SECRET_KEY required for minisign" >&2
            exit 1
        fi
        minisign -Sm "$MANIFEST" -s "$FORTIFY_SECRET_KEY"
        write_status "signed" "minisign" "${MANIFEST}.minisig" "" "true"
        echo "[FORTIFY-SIGN] minisign signature written: ${MANIFEST}.minisig"
        ;;

    gpg)
        if ! command -v gpg >/dev/null 2>&1; then
            echo "[FORTIFY-SIGN] gpg not on PATH" >&2
            exit 1
        fi
        if [ -z "${FORTIFY_GPG_KEY:-}" ]; then
            echo "[FORTIFY-SIGN] FORTIFY_GPG_KEY required for gpg" >&2
            exit 1
        fi
        gpg --batch --yes --local-user "$FORTIFY_GPG_KEY" \
            --output "${MANIFEST}.asc" --detach-sign --armor "$MANIFEST"
        write_status "signed" "gpg" "${MANIFEST}.asc" "" "true"
        echo "[FORTIFY-SIGN] gpg signature written: ${MANIFEST}.asc"
        ;;

    cosign)
        if ! command -v cosign >/dev/null 2>&1; then
            echo "[FORTIFY-SIGN] cosign not on PATH" >&2
            exit 1
        fi
        cosign sign-blob --yes "$MANIFEST" \
            --output-signature   "${MANIFEST}.sig" \
            --output-certificate "${MANIFEST}.cert"
        write_status "signed" "cosign" "${MANIFEST}.sig" "${MANIFEST}.cert" "true"
        echo "[FORTIFY-SIGN] cosign signature written"
        ;;

    *)
        echo "[FORTIFY-SIGN] unknown FORTIFY_SIGNER: $SIGNER" >&2
        exit 1
        ;;
esac
