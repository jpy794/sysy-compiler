#!/bin/bash

# usage: ./tool/bin_debug.sh path/to/bin path/to/input

bin=$1
stdin=$2
port=1234

if [ -z $stdin ]; then
    qemu-riscv64-static -L /usr/riscv64-linux-gnu -g $port $bin &
else
    qemu-riscv64-static -L /usr/riscv64-linux-gnu -g $port $bin < $stdin &
fi

riscv64-linux-gnu-gdb -q \
    -ex 'set sysroot /usr/riscv64-linux-gnu' \
    -ex "file $bin" \
    -ex "target remote localhost:$port" \
    -ex 'break main' \
    -ex 'continue'
