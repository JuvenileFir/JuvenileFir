#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk 'NR > 1 { total += $1 } END {print total/(NR-1)}' # leave out first line
}

DIR=logs/operator

FILE_F8_LT=f8_data_LT.txt
cp f5a_data.txt ${FILE_F8_LT}

FILE_F8_GT=f8_data_GT.txt
rm -f ${FILE_F8_GT}



for WIDTH in 32; do
    INPUT_FILE=${DIR}/OP_gt.log
    awk -v filename=/tmp/gt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
    for i in {1..11}; do
        file=/tmp/gt.log${i}
        cat ${file} | grep -a "gt time" > /tmp/foo
        # echo WIDTH: ${WIDTH} SELEC: $[(i-1)*10]
        get_mean_time /tmp/foo | tee -a ${FILE_F8_GT}
    done
done