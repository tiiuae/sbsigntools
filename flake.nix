{
  description = "sbsigntools - Signing utility for UEFI secure boot";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        sbsigntools = pkgs.callPackage ./package.nix {
          sbsigntoolsSrc = self;
        };
      in
      {
        packages = {
          default = sbsigntools;
          sbsigntools = sbsigntools;
        };

        devShells.default = pkgs.mkShell {
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
            echo "Run 'nix build' to build the Nix package"
          '';
        };

        apps.default = {
          type = "app";
          program = "${sbsigntools}/bin/sbsign";
        };
      });
}