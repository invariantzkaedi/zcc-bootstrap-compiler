#!/usr/bin/env bash
set -euo pipefail

MANIFEST="${MANIFEST:-artifacts/fortify-layout.subject.json}"
SIGNER="${FORTIFY_SIGNER:-}"
PUBKEY="${FORTIFY_VERIFY_KEY:-}"
REQUIRE_SIGNATURE="${FORTIFY_REQUIRE_SIGNATURE:-0}"

case "$SIGNER" in
  "")
    if [ "$REQUIRE_SIGNATURE" = "1" ]; then
      echo "FORTIFY_REQUIRE_SIGNATURE=1 but FORTIFY_SIGNER is not set"
      exit 1
    fi

    echo "[FORTIFY-VERIFY-SIGNATURE] skipped: FORTIFY_SIGNER not set"
    ;;

  minisign)
    if [ -z "$PUBKEY" ]; then
      echo "FORTIFY_VERIFY_KEY is required for minisign verification"
      exit 1
    fi

    minisign -Vm "$MANIFEST" -p "$PUBKEY"
    echo "[FORTIFY-VERIFY-SIGNATURE] minisign verified"
    ;;

  gpg)
    gpg --batch --verify "${MANIFEST}.asc" "$MANIFEST"
    echo "[FORTIFY-VERIFY-SIGNATURE] gpg verified"
    ;;

  cosign)
    cosign verify-blob "$MANIFEST" \
      --signature "${MANIFEST}.sig" \
      --certificate "${MANIFEST}.cert"

    echo "[FORTIFY-VERIFY-SIGNATURE] cosign verified"
    ;;

  *)
    echo "unknown FORTIFY_SIGNER: $SIGNER"
    exit 1
    ;;
esac