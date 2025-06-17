sbsigntool - Signing utility for UEFI secure boot

  Copyright (C) 2102 Jeremy Kerr <jeremy.kerr@canonical.com>

  Copying and distribution of this file, with or without modification,
  are permitted in any medium without royalty provided the copyright
  notice and this notice are preserved.

## Features

This version includes enhanced OpenSSL engine support for secure signing:

- **Hardware Security Module (HSM) Support**: Sign UEFI binaries using HSMs 
  without exposing private keys locally
- **Cloud Key Management**: Integration with cloud services like Azure Key Vault
- **PKCS#11 Token Support**: Use smartcards and hardware tokens for signing
- **Backward Compatibility**: Traditional file-based key operations remain unchanged

### Engine Usage Examples

Traditional file-based signing:
  sbsign --key private.pem --cert cert.pem --output signed.efi input.efi

HSM/Engine-based signing:
  sbsign --engine e_akv --keyform engine \
         --key "vault:keyvault-name:key-name" \
         --cert certificate.pem \
         --output signed.efi input.efi

PKCS#11 token signing:
  sbsign --engine pkcs11 --keyform engine \
         --key "pkcs11:token=MyToken;object=MyKey" \
         --cert certificate.pem \
         --output signed.efi input.efi

See file ./INSTALL for building and installation instructions.

For Nix users:
  # Build the package
  nix-build

  # Enter development shell
  nix-shell

  # With flakes enabled
  nix build
  nix develop
  nix run  # runs sbsign directly

Original development was done at:
  git://kernel.ubuntu.com/jk/sbsigntool.git

The current maintained fork resides at:

  https://git.kernel.org/pub/scm/linux/kernel/git/jejb/sbsigntools.git/

And a very low volume mailing list for bugs and patches is setup at

 sbsigntools@groups.io

Thanks to groups.io policies, non-members can post to this list, but
non-member postings are moderated until released (so they won't show
up immediately).  The list archives are available:

 https://groups.io/g/sbsigntools/topics

sbsigntool is free software.  See the file COPYING for copying conditions.
