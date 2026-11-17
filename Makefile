## Makefile — documentation
## Usage:
##   make            # builds kernel, schedulers, ouichefs, and vm, if the kernel does not exist, it will fetch the 7.2.15 version and extract it
##   make kernel     # builds the Linux kernel in the bundled tree
##   make sched      # builds sched_ext userspace & BPF schedulers
##   make clean      # clean build artifacts and generated images
##   make full_clean
##
## Important variables (can be overridden on the command line):
##   nproc       - number of parallel jobs (defaults to output of `nproc`)
##   kdir        - path to the Linux kernel source tree
##
## Typical examples:
##   make                # build everything using defaults
##   make nproc=8 kernel # build kernel with 8 parallel jobs
##   make kdir=/path/to/custom-linux sched

nproc          ?= $(shell nproc)
KERNEL_VER     := 7.2.15
DEFAULT_KDIR   := ./linux-$(KERNEL_VER)
kdir           ?= $(DEFAULT_KDIR)
KERNEL_SCX_DIR ?= $(kdir)/tools/sched_ext
kconfig        := $(kdir)/.config
TARBALL        := linux-$(KERNEL_VER).tar.xz
KERNEL_URL     := https://www.kernel.org/pub/linux/kernel/v7.x/$(TARBALL)

ouichefsdir    ?= ./ouichefs
output_vm      := ./output-vm

DEFAULT_TARGETS := scx_simple scx_cpu0 scx_qmap scx_central scx_flatcg scx_userland scx_pair scx_sdt
MY_TARGETS      := scx_mine scx_ksched scx_dyna
ALL_TARGETS     := $(DEFAULT_TARGETS) $(MY_TARGETS)

all: sched sched_vm

sched_vm:
	nix-build vm.nix -o $(output_vm)

# Build sched_ext schedulers (ensures kernel is built so vmlinux/BTF is available)
sched: $(kdir)/vmlinux
	cp ./src/schedulers/* $(KERNEL_SCX_DIR)
	$(MAKE) -j$(nproc) -C $(KERNEL_SCX_DIR) c-sched-targets="$(ALL_TARGETS)"

# Top-level phony target to build the kernel
kernel: $(kdir)/vmlinux

# 1. Download and extract kernel ONLY if $(kdir)/Makefile does not exist
$(kdir)/Makefile:
	@if [ "$(kdir)" != "$(DEFAULT_KDIR)" ]; then \
		echo "Error: Custom kdir '$(kdir)' does not exist or has no Makefile."; \
		exit 1; \
	fi
	[ -f $(TARBALL) ] || wget $(KERNEL_URL)
	tar xvf $(TARBALL)
	touch $@

# 2. Copy .config ONLY after kernel source tree exists
$(kconfig): $(kdir)/Makefile
	cp ./.config_sched_ext $@
	$(MAKE) -C $(kdir) olddefconfig

# 3. Compile the kernel ONLY if vmlinux is missing or .config changed
$(kdir)/vmlinux: $(kconfig)
	time $(MAKE) -C $(kdir) -j$(nproc)

# $(boot_disk):
# 	[ -f $(boot_disk).xz ] || wget https://etyloppihacilem.com/collection/$(boot_disk).xz
# 	xz -d $(boot_disk).xz

clean:
	rm -f $(output_vm)
	if [ -d "$(KERNEL_SCX_DIR)" ]; then \
		$(MAKE) -C $(KERNEL_SCX_DIR) c-sched-targets="$(ALL_TARGETS)" clean; \
	fi
	if [ -d "$(ouichefsdir)" ]; then \
		$(MAKE) -C $(ouichefsdir) KDIR=$(abspath $(kdir)) clean; \
	fi

full_clean:

clean:
	$(MAKE) -C $(kdir) clean
	rm ./nixos.qcow2
	rm -f $(output_vm)
	if [ -d "$(KERNEL_SCX_DIR)" ]; then \
		$(MAKE) -C $(KERNEL_SCX_DIR) c-sched-targets="$(ALL_TARGETS)" clean; \
	fi

compile_commands: clean
	bear -- $(MAKE)

.PHONY: ouichefs kernel clean all compile_commands sched sched_vm full_clean
