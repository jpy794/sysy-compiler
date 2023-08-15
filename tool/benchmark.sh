#!/bin/bash

# usage: ./tool/benchmark.sh ip port username password case_path tmp_path
# note: run make check_sysyc_asm_opt first to generate binaries

set -eu

ip=$1
port=$2
user=$3
pass=$4
case_path=$5
tmp_path=$6

ssh_pass="sshpass -p $pass"

$ssh_pass ssh -p $port ${user}@${ip} "mkdir -p /home/${user}/${tmp_path}"

echo copying binaries...
perf_bins=$(find build/test/sysyc/asm_opt_perf -type f ! -name "*.*")
$ssh_pass scp -P $port $perf_bins ${user}@${ip}:/home/${user}/${tmp_path}

echo running...
cat << EOF | $ssh_pass ssh -p $port ${user}@${ip} 'bash -s'
set -eu
cd /home/$user/$tmp_path
for bin in \$(ls); do
    if ls /home/$user/$case_path/\${bin}.in* &> /dev/null; then
        time=\$(cat /home/$user/$case_path/\$bin.in* | timeout 300s ./\$bin 2>&1 1>/dev/null | grep 'TOTAL:' | awk '{print \$2}')
    else
        time=\$(timeout 300s ./\$bin 2>&1 1>/dev/null | grep 'TOTAL:' | awk '{print \$2}')
    fi
    if [ -z "\$time" ]; then
        echo \$bin timeout/segfault
    else
        echo \$bin \$time
    fi
done
EOF
