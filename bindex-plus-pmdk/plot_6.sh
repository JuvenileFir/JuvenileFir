#!/bin/bash

get_mean_time() {
    cat $1 | grep -aoP "(?<= )[0-9.]+(?= ms)" | awk 'NR > 1 { total += $1 } END {print total/(NR-1)}' # leave out first line
}

DIR=logs/uniform

FILE_F6=f6_data.txt
rm -f ${FILE_F6}

# figure 7
for WIDTH in {4..32..4}; do
    file=${DIR}/UNIFORM_2_selc_10_b_${WIDTH}.log
    echo WIDTH: ${WIDTH}
    get_mean_time ${file} | tee -a ${FILE_F6}
    echo ===========================
done
