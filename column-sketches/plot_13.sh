#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk '{ total += $1 } END {print total/NR}'
}

DIR=logs/uniform

# figure 6
INPUT_FILE=${DIR}/F13.log
OUTPUT_FILE=f13_data.txt
rm -f ${OUTPUT_FILE}
awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
for i in {1..21}; do
file=/tmp/lt.log${i}
echo WIDTH: ${WIDTH} SELEC: $[(i-1)*10]
get_mean_time ${file} >> ${OUTPUT_FILE}
done
echo ====================
