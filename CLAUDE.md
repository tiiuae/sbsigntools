# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

sbsigntools is a utility for signing UEFI secure boot binaries. It provides tools to sign, verify, and manage signatures for EFI binaries and variables. The project recently added OpenSSL engine support to enable signing with HSMs and cloud key management services.

## Build Commands

### Initial Setup
```bash
# Prepare the build system (only needed once after clone)
./autogen.sh

# Configure the build
./configure

# Build the project
make
```

### Common Development Commands
```bash
# Clean build
make clean && make

# Run all tests
make check

# Run a specific test
make check TESTS=sign-verify.sh

# Debug build
./configure CFLAGS="-g -O0" && make

# Generate tags for code navigation
make tags
```

## Testing

### Run Unit Tests
```bash
make check
```

### Test Engine Functionality
```bash
# The testsbsign.sh script tests OpenSSL engine support with SoftHSM
./testsbsign.sh
```

### Individual Test Files
Tests are located in `tests/` directory and can be run individually:
- `sign-verify.sh` - Basic signing and verification
- `sign-detach-verify.sh` - Detached signature tests
- `sign-attach-verify.sh` - Attached signature tests
- Additional tests for error conditions and edge cases

## Architecture

### Core Components

1. **Signing Tools** (in `src/`):
   - `sbsign.c` - Sign EFI binaries (supports OpenSSL engines)
   - `sbverify.c` - Verify signed EFI binaries
   - `sbattach.c` - Attach signatures to EFI binaries
   - `sbvarsign.c` - Sign EFI variables
   - `sbsiglist.c` - Manage signature lists
   - `sbkeysync.c` - Synchronize keys

2. **Common Libraries**:
   - `image.c/h` - PE/COFF image handling
   - `idc.c/h` - Image digest calculation and signature handling
   - `fileio.c/h` - File I/O utilities
   - `coff/` - PE/COFF format headers

3. **Build System**:
   - GNU Autotools (configure.ac, Makefile.am)
   - Supports ia32, x86_64, arm, aarch64, riscv64

### OpenSSL Engine Support

The project supports OpenSSL engines for key operations, enabling:
- Hardware Security Module (HSM) usage
- Cloud key services (e.g., Azure Key Vault)
- PKCS#11 token support

Engine-related options in sbsign:
- `--engine <engine>` - Specify OpenSSL engine
- `--keyform <format>` - Key format (PEM or ENGINE)
- `--addcert <file>` - Add additional certificates

### Dependencies

- OpenSSL/libcrypto (preferably 3.0.0+)
- libuuid
- binutils development package (bfd.h)
- gnu-efi
- Standard build tools (gcc, make, pkg-config)

## Key Development Notes

1. **Engine Context Management**: Recent refactoring improved OpenSSL engine context handling to prevent memory leaks with PKCS#11 implementations.

2. **Certificate Chain**: When using engines, additional certificates may need to be provided via `--addcert` for proper chain building.

3. **Testing with Engines**: Use `testsbsign.sh` as a reference for testing engine functionality with SoftHSM.

4. **Error Handling**: The tools provide detailed error messages for signing failures, especially important when debugging engine issues.

5. **Backward Compatibility**: The tools maintain compatibility with traditional file-based key operations while supporting engine-based signing.