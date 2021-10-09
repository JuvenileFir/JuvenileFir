#!/bin/bash

make

PROGRAM="numactl --membind=0 ./column-sketches.out"

# create log dir
rm -rf logs/uniform
mkdir -p logs/uniform

# figure 6
# width 8 16 32
# selectivity 0 -> 1, 0.1
for WIDTH in 8 16 32; do
    CODE_WIDTH="WIDTH_${WIDTH}"

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in {0..100..10}; do
        TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
	if [[ $TARGET == 0 ]]; then
		TARGET=1
	fi
        TARGETS+=${TARGET}
        TARGETS+=","
    done

    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR} -w ${WIDTH}"
    OUTPUT_FILE="logs/uniform/F6_${WIDTH}.log"

    echo ${ARGS} ${CODE_WIDTH}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi
done

for WIDTH in 32; do
    CODE_WIDTH="WIDTH_${WIDTH}"

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for SELECTIVITY in {0..100..5}; do
        TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
	if [[ $TARGET == 0 ]]; then
		TARGET=1
	fi
        TARGETS+=${TARGET}
        TARGETS+=","
    done

    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR} -w ${WIDTH}"
    OUTPUT_FILE="logs/uniform/F13.log"

    echo ${ARGS} ${CODE_WIDTH}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi
done


# figure 7
# width 4 -> 32, 4
for WIDTH in {4..32..4}; do
    CODE_WIDTH="WIDTH_${WIDTH}"

    MAX_VAL=$[1<<${WIDTH}]
    TARGETS=""
    for _ in {1..100}; do
        TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
        TARGETS+=${TARGET}
        TARGETS+=","
    done

    OPERATOR="lt"

    ARGS="-l ${TARGETS} -o ${OPERATOR}"
    OUTPUT_FILE="logs/uniform/F7_${CODE_WIDTH}.log"

    echo ${ARGS} ${CODE_WIDTH}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

    if [ $? -ne 0 ]; then
        echo BAD!!!!${OUTPUT_FILE}
    fi
done
