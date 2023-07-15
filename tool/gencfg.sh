#!/bin/sh

# usage: ./tool/gencfg.sh [test.sy|test.ll] func_name [-O1]

source=$(realpath $1)
func=$2
sysyc_args=${@:3}

proj=$(pwd)
tmp=$(mktemp -d)

source_base=$(echo $(basename $source) | sed -E 's/\..*//')
source_suffix=$(echo $(basename $source) | grep -oE '\..*')
png=$source_base.$func.png

cd $tmp
if [ $source_suffix == '.sy' ]; then
    $proj/build/src/sysyc -S -emit-llvm $sysyc_args $source | opt --dot-cfg --disable-output
elif [ $source_suffix == '.ll' ]; then
    cat $source | opt --dot-cfg --disable-output
else
    echo 'expect *.ll or *.sy input'
    exit 1
fi
dot -Tpng .$func.dot -o $png
cp $png $proj/
