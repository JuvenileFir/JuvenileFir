#!/bin/bash

PROGRAM="numactl --membind=0 ./bindex"

# create log dir
rm -rf logs/uniform
mkdir -p logs/uniform

# 1
# width 8 16 32
# selectivity 0 -> 1, 0.1
for WIDTH in 8 16 32; do
    CODE_WIDTH="WIDTH_${WIDTH}"
    g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -o bindex

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in {0..100..10}; do
        TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
        TARGETS+=${TARGET}
        TARGETS+=","
    done

    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR} -s"
    OUTPUT_FILE="logs/uniform/UNIFORM_1_selc_${SELECTIVITY}_b_${WIDTH}.log"

    echo ${ARGS}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi
done


# 2
# width 4 -> 32, 4
# selectivity 0.1
for WIDTH in {4..32..4}; do
    CODE_WIDTH="WIDTH_${WIDTH}"
    g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -o bindex

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in 10; do
        TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
        TARGETS+=${TARGET}
        TARGETS+=","
    done

    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR}"
    OUTPUT_FILE="logs/uniform/UNIFORM_2_selc_${SELECTIVITY}_b_${WIDTH}.log"

    echo ${ARGS}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi
done
