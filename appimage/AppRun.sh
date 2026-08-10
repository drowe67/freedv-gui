#!/bin/bash -e
echo "In AppImage AppRun"
cd "$APPDIR"
export SSL_CERT_FILE="$APPDIR/etc/ssl/certs/ca-certificates.crt"
export SSL_CERT_DIR="$APPDIR/etc/ssl/certs"
echo "#### after import"
exec "$APPDIR/usr/bin/freedv" "$@"
