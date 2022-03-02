#!/bin/bash

PROGRAM="numactl --membind=0 ./bindex"

get_mean_time() {
    cat $1 | grep -oP "(?<= )[0-9.]+(?= ms)" | awk 'NR > 1 { total += $1 } END {print total/(NR-1)}' # leave out first line
}

# create log dir
rm -rf logs/varea
mkdir -p logs/varea

WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

# 1
# width 8 16 32
# selectivity 0 -> 1, 0.1
# for VA in 16 32 64 128 256 512; do
for VA in 16 32 128 512; do
# for VA in 16; do
    g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -D VAREA_N=${VA} -o bindex

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in {0..100..5}; do
        TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
        TARGETS+=${TARGET}
        TARGETS+=","
    done


    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR} -s"
    OUTPUT_FILE="logs/varea/VAREA_${VA}.log"

    echo ${ARGS} va${VA}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi

    awk -v filename=/tmp/f13.log 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${OUTPUT_FILE}
    rm -f f13_data${VA}.txt   
    for i in {1..21}; do
        file=/tmp/f13.log${i}
        cat ${file} | grep -a "lt time" > /tmp/foo
        # echo WIDTH: ${WIDTH} SELEC: $[(i-1)*10]
        get_mean_time /tmp/foo | tee -a f13_data${VA}.txt
    done
    echo ====================
done

cd ../column-sketches/
./plot_13.sh
