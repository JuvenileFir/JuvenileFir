#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk '{ total += $1 } END {print total/NR}'
}

DIR=logs/uniform

# figure 7
OUTPUT_FILE=f6_data.txt
rm -f ${OUTPUT_FILE}
for WIDTH in {4..32..4}; do
    file=${DIR}/F7_WIDTH_${WIDTH}.log
    echo WIDTH: ${WIDTH}
    get_mean_time ${file} >> ${OUTPUT_FILE}
    echo ===========================
done
