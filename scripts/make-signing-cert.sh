#!/bin/bash
# Creates a permanent self-signed code-signing certificate «Klats Signing» in the login keychain.
#
# Why: macOS remembers the Accessibility permission by the app's signature. An ad-hoc signature
# changes with every build, so the permission is lost after each update. With one fixed
# certificate the signature's designated requirement stays the same across builds, and the
# permission survives updates, for the developer and for every user.
#
# The certificate is not trusted by Apple, so downloads still need «Open Anyway» once. That is
# unchanged from ad-hoc signing.
set -euo pipefail
NAME="Klats Signing"
KEYCHAIN="$HOME/Library/Keychains/login.keychain-db"

if security find-certificate -c "$NAME" "$KEYCHAIN" >/dev/null 2>&1; then
    echo "certificate «$NAME» already exists in the login keychain"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/openssl.cnf" <<CNF
[req]
distinguished_name = dn
x509_extensions = ext
prompt = no
[dn]
CN = $NAME
O = Klats
[ext]
keyUsage = critical, digitalSignature
extendedKeyUsage = critical, codeSigning
basicConstraints = critical, CA:false
subjectKeyIdentifier = hash
CNF

openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \
    -keyout "$TMP/key.pem" -out "$TMP/cert.pem" -config "$TMP/openssl.cnf" 2>/dev/null
# The keychain only reads the older PKCS#12 encryption, not OpenSSL 3's defaults.
openssl pkcs12 -export -inkey "$TMP/key.pem" -in "$TMP/cert.pem" -name "$NAME" \
    -keypbe PBE-SHA1-3DES -certpbe PBE-SHA1-3DES -macalg sha1 \
    -out "$TMP/klats.p12" -passout pass:klats
# -T lets codesign use the private key without asking every time.
security import "$TMP/klats.p12" -k "$KEYCHAIN" -P klats -T /usr/bin/codesign -T /usr/bin/security >/dev/null
echo "certificate «$NAME» created in the login keychain (valid 10 years)"

# Prove it signs, and show what macOS will remember the app by.
cp /bin/ls "$TMP/probe"
codesign --force --sign "$NAME" --identifier io.github.belokoz.klats.probe "$TMP/probe" 2>&1 | grep -v "^$" || true
codesign -d -r- "$TMP/probe" 2>&1 | grep designated
