#!/bin/bash

case_path=$1
out_path=$2
case_name=$3

case_full=$case_path/$case_name

# avoid stack overflow due to recursion (perf/median2.sy)
ulimit -s unlimited

if [ -f $case_path/$case_name.in ]; then
    $out_path/$case_name < $case_path/$case_name.in 1> $out_path/$case_name.stdout 2> $out_path/$case_name.stderr
else
    $out_path/$case_name 1> $out_path/$case_name.stdout 2> $out_path/$case_name.stderr
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
