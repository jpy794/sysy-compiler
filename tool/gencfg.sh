#!/bin/sh

# usage: ./tool/gencfg.sh test.sy func_name [-O1]

sy=$(realpath $1)
func=$2
extra_flag=$3

proj=$(pwd)
tmp=$(mktemp -d)

sy_base=$(echo $(basename $sy) | sed -e 's/\.sy//')
png=$sy_base.$func.png

make -C $proj/build -j$(nproc)

cd $tmp
$proj/build/src/sysyc -S -emit-llvm $extra_flag $sy | opt --dot-cfg --disable-output
dot -Tpng .$func.dot -o $png
cp $png $proj/
