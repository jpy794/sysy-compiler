#!/bin/bash

# usage: ./tool/bin_debug.sh path/to/bin

bin=$1
port=1234

qemu-riscv64-static -L /usr/riscv64-linux-gnu -g $port $bin &
riscv64-linux-gnu-gdb -q \
    -ex 'set sysroot /usr/riscv64-linux-gnu' \
    -ex "file $bin" \
    -ex "target remote localhost:$port" \
    -ex 'break main' \
    -ex 'continue'
