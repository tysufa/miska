# vm.nix
{ pkgs ? import <nixpkgs> {} }:
(import <nixpkgs/nixos> {
  configuration = { config, pkgs, modulesPath, ... }: {
    imports = [ "${modulesPath}/virtualisation/qemu-vm.nix" ];

    boot.kernelPackages = pkgs.linuxPackages_latest;
    services.getty.autologinUser = "root";

    virtualisation = {
      memorySize = 2048;
      cores = 4;
      graphics = false;
      sharedDirectories.work = {
        source = toString ./.;
        target = "/mnt/work";
        writable = true;
      };
    };
    virtualisation.forwardPorts = [ { from = "host"; host.port = 2222; guest.port = 22; } ];

    environment.systemPackages = with pkgs; [ stress-ng bpftools gcc perf neovim ];

    programs.nix-ld = {
      enable = true;
      libraries = with pkgs; [
        stdenv.cc.cc
        elfutils
        zlib
      ];
    };

    services.openssh = {
      enable = true;
      settings = {
        PermitRootLogin = "yes";
        PermitEmptyPasswords = "yes"; # Convenient for local ephemeral test VMs
      };
    };
    security.pam.services.sshd.allowNullPassword = true;
    users.users.root.initialHashedPassword = "";


  };
}).vm
