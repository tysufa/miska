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

    shellHook = ''
        echo "Entering Sched_ext Nix development environment"

        # Export BPF compiler paths to make life easier for Makefiles
        # export CLANG=${pkgs.clang}/bin/clang
        # export LLC=${pkgs.llvm}/bin/llc

        CONF_DIR=$(mktemp -d)
        if [ -f "$HOME/.zshrc" ]; then
          echo "source \"$HOME/.zshrc\"" > "$CONF_DIR/.zshrc"
        elif [ -f "$HOME/.config/zsh/.zshrc" ]; then
          echo "source \"$HOME/.config/zsh/.zshrc\"" > "$CONF_DIR/.zshrc"
        fi

        cat << 'EOF' >> "$CONF_DIR/.zshrc"
        export HISTFILE="$PWD/.nix_history"
        zshexit() {
          echo -e "\n[Nix] Environment unloaded. Back to normal!"
        }
        EOF

        export ZDOTDIR="$CONF_DIR"
        exec zsh
      '';

  in {
    devShells.${system} = {
      default = pkgs.mkShell {
        # nativeBuildInputs: Tools executed on the host during build time
        hardeningDisable = [ "all" ]; # Disables Nix's injected _FORTIFY_SOURCE flag and more

        nativeBuildInputs = with pkgs; [
          # --- Core eBPF & LLVM Toolchain ---
          clang
          llvm
          bpftools # Used to generate vmlinux.h and BPF skeletons
          pkg-config
          gnumake
          python3 # needed to compile sched_ext
          libbpf
          pahole


          wget

          # --- Kernel Build Essentials ---
          bison
          flex
          bc
          elfutils # libelf utilities
          openssl # For module signing/crypto
          util-linux
          bear # Generates compile_commands.json
          gdb

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

        shellHook = shellHook;
      };

      python = pkgs.mkShell {
        name = "python";
        nativeBuildInputs = [
          (pkgs.python3.withPackages (python-pkgs: [
            python-pkgs.matplotlib
            python-pkgs.pandas
            python-pkgs.numpy
          ]))
        ];

        shellHook = shellHook;
      };
    };
  };
}
