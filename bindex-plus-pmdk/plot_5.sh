#!/bin/bash

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk 'NR > 1 { total += $1 } END {print total/(NR-1)}' # leave out first line
}

echo "ATTENTION! It is REQUIRED to run the scrips STEP-BY-STEP in the order (Fig5-Fig14) in readme.pdf. If you skip the generation of data for some figures, the following figures may not be generated correctly."

DIR=logs/uniform

rm -f 8.txt 16.txt 32.txt

# figure 5
for WIDTH in 8 16 32; do
    INPUT_FILE=${DIR}/UNIFORM_1_selc_100_b_${WIDTH}.log
    awk -v filename=/tmp/lt.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${INPUT_FILE}
    for i in {1..11}; do
        file=/tmp/lt.log${i}
        cat ${file} | grep -a "lt time" > /tmp/foo
        # echo WIDTH: ${WIDTH} SELEC: $[(i-1)*10]
        get_mean_time /tmp/foo | tee -a ${WIDTH}.txt
    done
    echo ====================
done

cp 32.txt f5a_data.txt
cp 16.txt f5b_data.txt
cp 8.txt f5c_data.txt
