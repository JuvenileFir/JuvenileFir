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
for VA in 16 32 64 128 256 512; do
# for VA in 16 32 128 512; do
# for VA in 16; do
    g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -D VAREA_N=${VA} -o bindex

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in {0..100..10}; do
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
    
    DATA_FILE=f14_data${VA}.txt
    rm -f ${DATA_FILE}
    cat ${OUTPUT_FILE} | grep "copy" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}
    cat ${OUTPUT_FILE} | grep "refine" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}
done
