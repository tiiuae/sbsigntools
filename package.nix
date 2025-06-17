{ lib
, stdenv
, fetchFromGitHub
, fetchgit
, autoreconfHook
, pkg-config
, openssl
, libuuid
, gnu-efi
, help2man
, binutils-unwrapped
, git
, sbsigntoolsSrc ? null
}:

stdenv.mkDerivation rec {
  pname = "sbsigntools";
  version = "0.9.5-ssrc";

  src = if sbsigntoolsSrc != null then sbsigntoolsSrc else (builtins.filterSource (path: type: baseNameOf path != ".git") ./.);

  # Fetch ccan submodule
  ccan = fetchgit {
    url = "git://git.ozlabs.org/~ccan/ccan";
    rev = "b1f28e17227f2320d07fe052a8a48942fe17caa5";
    sha256 = "sha256-VUK+6VxCNhwh+7Tvky6GA5elrTIiFUfBtUBD/tq6o6A=";
  };

  postUnpack = ''
    # Create lib directory if it doesn't exist
    mkdir -p $sourceRoot/lib
    
    # Remove empty submodule directory if it exists
    if [ -d "$sourceRoot/lib/ccan.git" ]; then
      rm -rf $sourceRoot/lib/ccan.git
    fi
    
    # Copy ccan source
    cp -r ${ccan} $sourceRoot/lib/ccan.git
    chmod -R u+w $sourceRoot/lib/ccan.git
    
    # Create symlink for autotools (some parts expect lib/ccan)
    ln -sf ccan.git $sourceRoot/lib/ccan
    
    # Create missing files that autotools expects
    touch $sourceRoot/AUTHORS
    touch $sourceRoot/ChangeLog
    
    # Create a minimal Makefile.am for ccan if it doesn't exist
    if [ ! -f "$sourceRoot/lib/ccan.git/Makefile.am" ]; then
      cat > $sourceRoot/lib/ccan.git/Makefile.am << 'EOF'
# Minimal Makefile.am for ccan
SUBDIRS = 
noinst_LIBRARIES = libccan.a
libccan_a_SOURCES = ccan/talloc/talloc.c ccan/read_write_all/read_write_all.c
EOF
    fi
  '';

  nativeBuildInputs = [
    autoreconfHook
    pkg-config
    help2man
    git
  ];

  buildInputs = [
    openssl
    libuuid
    gnu-efi
    # Add binutils for bfd.h
    binutils-unwrapped
  ];

  configureFlags = [
    "--with-gnu-efi=${gnu-efi}"
    "--with-gnu-efi-lib=${gnu-efi}/lib"
    "--with-gnu-efi-inc=${gnu-efi}/include/efi"
    "--with-gnu-efi-crt=${gnu-efi}/lib"
  ];

  # The autogen.sh essentially does autoreconf
  preConfigure = ''
    patchShebangs .
    
    # Set up EFI architecture mapping
    export EFI_ARCH=${stdenv.hostPlatform.linuxArch}
    case "$EFI_ARCH" in
      i386) export EFI_ARCH=ia32 ;;
      x86_64) export EFI_ARCH=x86_64 ;;
      aarch64) export EFI_ARCH=aarch64 ;;
      arm) export EFI_ARCH=arm ;;
      armv*) export EFI_ARCH=arm ;;
      riscv64) export EFI_ARCH=riscv64 ;;
    esac
    
    # Override the hardcoded crt path detection by setting CRTPATH
    export CRTPATH=${gnu-efi}/lib
    
    # Override EFI include paths
    export EFI_CPPFLAGS="-I${gnu-efi}/include/efi -I${gnu-efi}/include/efi/$EFI_ARCH -DEFI_FUNCTION_WRAPPER"
  '';

  # Help2man might fail if not in PATH during build
  preBuild = ''
    export PATH="${help2man}/bin:$PATH"
  '';

  enableParallelBuilding = true;

  # Don't run tests by default as they require specific setup
  doCheck = false;

  meta = with lib; {
    description = "Signing utility for UEFI secure boot";
    longDescription = ''
      sbsigntools is a set of utilities for creating and verifying UEFI
      Secure Boot signatures. It includes tools to sign PE/COFF binaries
      for use with UEFI Secure Boot, verify signatures, manage signature
      lists, and sign EFI variables. The tools support OpenSSL engines
      for HSM and cloud key management integration.
    '';
    homepage = "https://git.kernel.org/pub/scm/linux/kernel/git/jejb/sbsigntools.git/";
    license = licenses.gpl3Plus;
    platforms = [ "x86_64-linux" "i686-linux" "aarch64-linux" "armv7l-linux" ];
    maintainers = with maintainers; [ ];
  };
}
