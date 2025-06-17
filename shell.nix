{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    autoreconfHook
    pkg-config
    help2man
    gnumake
    gcc
    
    # Dependencies
    openssl
    libuuid
    gnu-efi
    
    # Development tools
    gdb
    valgrind
    git
    
    # For testing with engines
    softhsm
    opensc
  ];

  shellHook = ''
    echo "sbsigntools development environment"
    echo "Run './autogen.sh && ./configure && make' to build"
    echo "Run 'make check' to run tests"
  '';
}