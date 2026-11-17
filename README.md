# Miska

## What is miska

It's a repository containing sched_ext schedulers, who aim to priorize lower pid tasks, there are 4 schedulers :
- scx_ksched
- scx_mine
- scx_kdyma
- scx_dyna

### scx_ksched and scx_mine
Those 2 are really basic : they only look at the smallest pid eligible and elect it, the main difference between the 2 is that scx_ksched runs almost entirely in the kernel (yes I know, great creativity in the name) in bpf code and scx_mine is almost entirely in userspace.
We made those 2 to learn how to make sched_ext schedulers and to compare the impact in terms of performance and ease of developement between userspace programming and kernel programming.

### scx_dyna

![scx_dyna algorithm](./rapport/images/png/schema_files_actives.png)

### scx_kdyna

$$ priority = c * b + (PID / (1 + \alpha))
\\
\alpha = 0.001 \\
b = PID\_MAX * \alpha \\
c = cycle\_number $$


## Setup and Test

### Installing

The schedulers have only be tested for kernel 7.2.15, it should work on most kernel with sched_ext support but some changes in the api could make the compilation fail.

#### If you are on nixos, the process is easy :
1. enter the nix shell with `nix develop`
2. run `make`
3. start the vm with `./run` or just by running the script in `./output-vm/bin/run-nixos-vm`

Once in the vm, you just have to compile the benchmark (`gcc /mnt/work/src/benchmark.c -o bench`) and execute it with or without the schedulers (accessible via `/mnt/work/linux-7.2.15/tools/sched_ext/build/bin/scx_ksched`)

The `vm.nix` script has only be tested on my machine, so if it doesn't work for you, feel free to modify it or use you own vm

#### Other plateforms

`make sched` should work properly and compile the linux kernel and sched_ext schedulers (has long as you have the required dependencies) but you will have to use/build your own virtual machine. 

### Using and testing
The schedulers are set with the flag `SCX_OPS_SWITCH_PARTIAL` which means unless the program notifies that it wants to use sched_ext, the kernel will keep using EEVDF. You can either remove this flag (unrecommanded) or use the provided benchmark : `src/benchmark.c`

to use it simply compile it and run it with the desired scheduler running. The schedulers are supposed to give metrics in stdout, but it is recommended to use perf instead as we didn't spend much time checking if the generated perfs are coherent and for now there is only the average_delay of the scheduler.
Simply use `perf sched record -- ./benchmark` and then `perf sched latency -p` to inspect the results
You can see an exemple of installing and setting up the repo (with a precompiled kernel to gain time) :

![./assets/miska_test.gif]
