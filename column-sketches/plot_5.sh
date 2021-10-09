#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk '{ total += $1 } END {print total/NR}'
}

DIR=logs/uniform

# figure 6
for WIDTH in 8 16 32; do
    INPUT_FILE=${DIR}/F6_${WIDTH}.log
    OUTPUT_FILE=F5_${WIDTH}.log
    rm -f ${OUTPUT_FILE}
    awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
    for i in {1..11}; do
        file=/tmp/lt.log${i}
        echo WIDTH: ${WIDTH} SELEC: $[(i-1)*10]
        get_mean_time ${file} >> ${OUTPUT_FILE}
    done
    echo ====================
done

mv F5_8.log f5a_data.txt
mv F5_16.log f5b_data.txt
mv F5_32.log f5c_data.txt
