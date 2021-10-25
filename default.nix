let
  default_nixpkgs = (import <nixpkgs> {}).fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs";
    rev = "13f10e9fe84dda019bd83201ac1e5e2c846e3840";
    sha256 = "01bsghmz2ankv51izxhvv953mgxk4s0zlk5i1qzj6pc9jkbhig6h";
  };
in
{ nixpkgs ? default_nixpkgs }:

with import nixpkgs {};
rec {
  llvm = (llvmPackages_12.libllvm.override {
    debugVersion = true;
  }).overrideAttrs (o: {
    # Alive2 requires LLVM to have RTTI and exception support, which aren't
    # normally enabled.
    cmakeFlags = o.cmakeFlags ++ [
      "-DLLVM_ENABLE_RTTI=ON"
      "-DLLVM_ENABLE_EH=ON"
    ];
    doCheck = false; # reduce build time

    #src = fetchFromGitHub {
    #  # LLVM 12.0 pre-release
    #  owner = "llvm";
    #  repo = "llvm-project";
    #  rev = "8e464dd76befbc4a39a1d21968a3d5f543e29312";
    #  sha256 = "17qlpq8gy6kqnnsbq264wgsylim55v2s17v306acvmmcr8h4gcg3";
    #};
    #unpackPhase = null;
    #sourceRoot = "source/llvm";
  });

  z3 = (import nixpkgs {}).z3.overrideAttrs (o: {
    src = fetchFromGitHub {
      # 2021-08-02
      owner  = "Z3Prover";
      repo   = "z3";
      rev    = "d3194bb8a8263e7a69459838b2100924fb441f9a";
      sha256 = "10vwx4ahnzyacr1hia3jdqvn8n9m5gzv7jrrrfb1wxm9mzmcjww9";
    };
  });



  alive2 = callPackage nix/alive2 {
    inherit llvm z3;
  };


  # Singularity container (to be run on HTCondor cluster)
  alive-worker-singularity = singularity-tools.buildImage {
    name = "alive-worker";
    contents = [ pkgs.busybox ];
    diskSize = 4096;
    runScript = ''
      #!/bin/sh
      set +e
      for i in $(seq 4); do
        while true; do
          echo starting worker...
          ${alive2}/bin/alive-worker "$@"
          echo exit code: $?
        done &
      done
      sleep 7d
      kill $(jobs -p)
    '';
  };
}
