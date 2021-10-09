#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk '{ total += $1 } END {print total/NR}'
}

DIR=logs/operator

rm -f f10_data.txt

# figure 15, 16
# a < x < b
echo lt
file=${DIR}/OP_bt.log
get_mean_time ${file} >> f10_data.txt
echo

# figure 15, 16
# a < x < b
echo bt
file=${DIR}/OP_lt.log
get_mean_time ${file} >> f10_data.txt
echo


echo =uniform
file=${DIR}/OP_eq_uniform.log
get_mean_time ${file} >> f10_data.txt


echo =skewed
file=${DIR}/OP_eq_skewed.log
get_mean_time ${file} >> f10_data.txt
