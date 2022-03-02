#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk 'NR > 1 { total += $1 } END {print total/(NR-1)}' # leave out first line
}

DIR=logs/zipf

# figure 10
# 1
for SKE in 1 2; do
    INPUT_FILE=${DIR}/ZIPF_${SKE}.log
    awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
    for i in {1..11}; do
        file=/tmp/lt.log${i}
        echo SKE: ${SKE} SELEC: $[(i-1)*10]
        get_mean_time ${file} | tee -a 9_${SKE}.txt
    done
    echo ====================
done

mv 9_1.txt f9b_data.txt
mv 9_2.txt f9c_data.txt
rm -f f9a_data.txt

mean_time_across_selc() {
    cat $1 | awk '{total += $1} END {print total/NR}'
}
mean_time_across_selc f5a_data.txt >> f9a_data.txt
mean_time_across_selc f9b_data.txt >> f9a_data.txt
mean_time_across_selc f9c_data.txt >> f9a_data.txt


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
