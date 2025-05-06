#!/bin/bash
set -e

# Install required packages
# sudo apt-get update
# sudo apt-get install -y softhsm2 libengine-pkcs11-openssl openssl sbsigntool opensc

# Initialize SoftHSM
rm -rf ~/.softhsm/tokens
mkdir -p ~/.softhsm/tokens
cat > ~/.config/softhsm2.conf << EOF
directories.tokendir = $HOME/.softhsm/tokens
objectstore.backend = file
log.level = INFO
slots.removable = false
EOF
export SOFTHSM2_CONF=~/.config/softhsm2.conf

softhsm2-util --show-slots

softhsm2-util --init-token --slot 0 --label "SecureBootToken" --pin 1234 --so-pin 5678

# Configure OpenSSL
cat > ~/softhsm-openssl.cnf << EOF
openssl_conf = openssl_init

[openssl_init]
engines = engine_section

[engine_section]
pkcs11 = pkcs11_section

[pkcs11_section]
engine_id = pkcs11
dynamic_path = /usr/lib/x86_64-linux-gnu/engines-1.1/pkcs11.so
MODULE_PATH = /usr/lib/softhsm/libsofthsm2.so
init = 0
PIN = 1234
EOF

# Test engine
export OPENSSL_CONF=~/softhsm-openssl.cnf
openssl engine -t -c pkcs11

# Generate keys and certificates
pkcs11-tool --module /usr/lib/softhsm/libsofthsm2.so --login --pin 1234 \
    --keypairgen --key-type rsa:2048 --id 01 --label "SecureBootKey"

# Create certificate
export OPENSSL_CONF=~/softhsm-openssl.cnf
openssl req -engine pkcs11 -keyform engine -key "pkcs11:object=SecureBootKey;type=private;pin-value=1234" \
    -new -x509 -days 3650 -out secureboot.pem -sha256 \
    -subj "/C=US/O=Your Organization/CN=Secure Boot Signing Key"

# Convert to DER format
openssl x509 -in secureboot.pem -outform DER -out secureboot.der

# Sign your EFI binary (replace with your actual EFI binary path)
cp /usr/lib/grub/x86_64-efi/monolithic/grubx64.efi ./test-unsigned.efi

../sbsigntools/src/sbsign --engine pkcs11 \
    --key "pkcs11:object=SecureBootKey;type=private;pin-value=1234" \
    --cert secureboot.pem \
    --output test-signed.efi \
    test-unsigned.efi

# Verify signature
sbverify --cert secureboot.pem test-signed.efi

echo "All steps completed successfully!"
echo "To enroll the certificate in UEFI firmware, run: sudo mokutil --import secureboot.der"

