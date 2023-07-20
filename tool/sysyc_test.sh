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
    
    clang -Wno-override-module $sysyc_out ./test/lib/sylib.c -o $out_path/$case_name

    if [ $? != 0 ]; then
        echo -e "\033[31m$case_full failed\033[0m"
        echo 'clang compile error'
        echo "see sysyc output file $sysyc_out"
        exit 1
    fi

    ./tool/bin_test.sh $case_path $out_path $case_name

else

    riscv64-linux-gnu-gcc $sysyc_out ./test/lib/sylib.c -o $out_path/$case_name

    if [ $? != 0 ]; then
        echo -e "\033[31m$case_full failed\033[0m"
        echo 'riscv64-linux-gnu-gcc compile error'
        echo "see sysyc output file $sysyc_out"
        exit 1
    fi

    docker run --rm --platform=linux/riscv64 \
        -v $(realpath $out_path):/test/out \
        -v $(realpath $case_path):/test/case \
        -v $(pwd)/tool:/test/tool \
        debian:unstable \
        /bin/bash -c "/test/tool/bin_test.sh /test/case /test/out $case_name" 

fi
