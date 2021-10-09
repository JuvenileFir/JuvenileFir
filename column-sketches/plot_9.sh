#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk '{ total += $1 } END {print total/NR}'
}

get_mean_time_safe() {
    cat $1 | awk '{ total += $1 } END {print total/NR}'
}

DIR=logs/zipf

# figure 10
# 1
for SKE in 1 2; do
    OUTPUT_FILE=F9-${SKE}.log
    rm -f ${OUTPUT_FILE}
    INPUT_FILE=${DIR}/ZIPF_${SKE}.log
    awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
    for i in {1..11}; do
        file=/tmp/lt.log${i}
        echo SKE: ${SKE} SELEC: $[(i-1)*10]
        get_mean_time ${file} >> ${OUTPUT_FILE}
    done
    echo ====================
done

mv F9-1.log f9b_data.txt
mv F9-2.log f9c_data.txt

rm -f f9a_data.txt
get_mean_time_safe f5a_data.txt >> f9a_data.txt
get_mean_time_safe f9b_data.txt >> f9a_data.txt
get_mean_time_safe f9c_data.txt >> f9a_data.txt

# figure 10
# 2
# for SKE in 1 2; do
#     INPUT_FILE=${DIR}/ZIPF_${SKE}_1.log
#     awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
#     for i in {1..11}; do
#         file=/tmp/lt.log${i}
#         echo SKE: ${SKE}_1 SELEC: $[(i-1)*10]
#         get_mean_time ${file}
#     done
#     echo ====================
# done
