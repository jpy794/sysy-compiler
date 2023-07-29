#!/bin/bash

# usage ./tool/sysc_test.sh path/to/case path/to/outdir [-emit-llvm|-O1]

set -u

case_full=$1
out_path=$2
sysyc_args=${@:3}

if [ ! -d $out_path ]; then
    mkdir -p $out_path
fi

emit_llvm=false

for i in $sysyc_args; do
    if [ $i == '-emit-llvm' ]; then
            emit_llvm=true
        break
    fi
done

case_path=$(dirname $case_full)
case_name=$(basename $case_full)
case_name=${case_name%.*}

if [ $emit_llvm == true ]; then
    sysyc_out=$out_path/$case_name.ll
else
    sysyc_out=$out_path/$case_name.s
fi

./build/src/sysyc -S $sysyc_args $case_full > $sysyc_out

if [ $? != 0 ]; then
    echo -e "\033[31m$case_full failed\033[0m"
    echo 'sysyc compile error'
    exit 1
fi

if [ $emit_llvm == true ]; then
    run_bin=$out_path/$case_name
    clang -Wno-override-module $sysyc_out ./test/lib/sylib.c -o $out_path/$case_name
else
    run_bin="qemu-riscv64-static -s 128M -L /usr/riscv64-linux-gnu $out_path/$case_name"
    riscv64-linux-gnu-gcc $sysyc_out ./test/lib/sylib.c -o $out_path/$case_name
fi

ret=$?

if [ $ret != 0 ]; then
    echo -e "\033[31m$case_full failed\033[0m"
    echo 'llvm ir / asm compile error'
    echo "see sysyc output file $sysyc_out"
    exit 1
fi

# avoid stack overflow due to recursion (perf/median2.sy)
ulimit -s unlimited

if [ -f $case_path/$case_name.in ]; then
    $run_bin < $case_path/$case_name.in 1> $out_path/$case_name.stdout 2> $out_path/$case_name.stderr
else
    $run_bin 1> $out_path/$case_name.stdout 2> $out_path/$case_name.stderr
fi

ret=$?

# append \n to stdout if missing
sed -e '$a\' $out_path/$case_name.stdout | cat > $out_path/$case_name.out
echo $ret >> $out_path/$case_name.out
diff -u --color $out_path/$case_name.out $case_path/$case_name.out &> /dev/null

if [ $? != 0 ]; then
    echo -e "\033[31m$case_full failed\033[0m"
    echo 'output is different'
    echo "see $out_path/$case_name.out, $case_path/$case_name.out for difference"
    exit 1
fi

echo -e "\033[32m$case_full passed\033[0m"
