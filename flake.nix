{
  description = "Sched_ext build environment";

  inputs = {
    nixpkgs.url = "github:Nixos/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
  in {
    devShells.${system}.default = pkgs.mkShell {
      # nativeBuildInputs: Tools executed on the host during build time
      nativeBuildInputs = with pkgs; [
        # --- Core eBPF & LLVM Toolchain ---
        clang
        llvm
        bpftools # Used to generate vmlinux.h and BPF skeletons
        pkg-config
        gnumake

        # --- Kernel Build Essentials ---
        bison
        flex
        bc
        elfutils # libelf utilities
        openssl # For module signing/crypto
        util-linux
        bear # Generates compile_commands.json

        # --- Debugging & Virtualization ---
        qemu_full
        gdb
        dbus # for qemu environments
      ];

      # buildInputs: Libraries that the target programs link against
      buildInputs = with pkgs; [
        elfutils # Provides <libelf.h>
        zlib # Required by libelf / bootstrap environments
        libbpf # Core userspace BPF loading library
        ncurses # Required if running 'make menuconfig'
      ];

      shellHook = ''
        echo "Entering Sched_ext Nix development environment"

        # Export BPF compiler paths to make life easier for Makefiles
        export CLANG=${pkgs.clang}/bin/clang
        export LLC=${pkgs.llvm}/bin/llc

        # --- Your Custom Zsh History & Environment Wrapper ---
        CONF_DIR=$(mktemp -d)
        if [ -f ~/.zshrc ]; then
          echo "source ~/.zshrc" > "$CONF_DIR/.zshrc"
        fi

        echo "export HISTFILE=\"$PWD/.nix_history\"" >> "$CONF_DIR/.zshrc"
        export ZDOTDIR="$CONF_DIR"

        # Launch into your custom Zsh session
        exec zsh

        # Note: 'exec zsh' replaces the shell process, so everything below
        # will handle cleanup beautifully when you type 'exit'.
        rm -rf "$CONF_DIR"
        echo -e "\n[Nix] Environment unloaded. Back to normal!"
      '';
    };
  };
}
