let
  default_nixpkgs = (import <nixpkgs> {}).fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs";
    rev = "f712cdd62e0e6763897096e62627f72061b2e6a3";
    sha256 = "s241WS3CI8470TOi9pLRQMzVdyqMEdOZXM3PKwIL6zM=";
  };

  ehLLVM = llvm: llvm.overrideAttrs (o: {
    # Alive2 requires LLVM to have RTTI and exception support, which aren't
    # normally enabled.
    cmakeFlags = o.cmakeFlags ++ [
      "-DLLVM_ENABLE_EH=ON"
      "-DLLVM_ENABLE_RTTI=ON"
    ];
    doCheck = false; # reduce build time
  });

  assertLLVM = llvm: llvm.overrideAttrs (o: {
    # Enable LLVM's assertions to catch bugs in the way Alive2 uses it.
    cmakeFlags = o.cmakeFlags ++ [
      "-DLLVM_ENABLE_ASSERTIONS=ON"
      "-DLLVM_BUILD_TESTS=OFF"
    ];
    doCheck = false; # reduce build time
  });
in
{ nixpkgs ? default_nixpkgs }:

with import nixpkgs {};
rec {
  llvm = assertLLVM (ehLLVM llvmPackages_14.libllvm);

  z3 = (import nixpkgs {}).z3.overrideAttrs (o: {
    src = fetchFromGitHub {
      # 2022-04-29
      owner  = "Z3Prover";
      repo   = "z3";
      rev    = "5a9b0dd747a2fc26513fcb15181678781e6667a7";
      sha256 = "vMq8iknvGpxxOmdoFFlQg1c+ML9DvXLHMNh0Ss4WsDQ=";
      fetchSubmodules = true;
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
