#!/bin/bash
WIDTH=32
MAX_VAL=$[1<<${WIDTH}]
PROGRAM="numactl --membind=0 ./column-sketches.out"

ZIPF_2_PATH=../data/zipf_2_32_1e9.data

# create log dir
rm -rf logs/operator
mkdir -p logs/operator

# figure 14
# x > a
# selectivity 0:100:10
# 32 bit
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for SELECTIVITY in {0..100..10}; do
    TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
    TARGETS+=${TARGET}
    TARGETS+=","
done

OPERATOR="gt"

ARGS="-l ${TARGETS} -o ${OPERATOR}"
OUTPUT_FILE="logs/operator/OP_gt.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

# figure 16
# x < a
# 32 bit
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for _ in {1..100}; do
    TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
    TARGETS+=${TARGET}
    TARGETS+=","
done
TARGET_1=${TARGETS}

OPERATOR="lt"

ARGS="-l ${TARGET_1} -o ${OPERATOR}"
OUTPUT_FILE="logs/operator/OP_lt.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

# figure 16
# a < x < b
# 32 bit
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for _ in {1..100}; do
    TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
    TARGETS+=${TARGET}
    TARGETS+=","
done
TARGET_1=${TARGETS}
TARGETS=""
for _ in {1..100}; do
    TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
    TARGETS+=${TARGET}
    TARGETS+=","
done
TARGET_2=${TARGETS}

OPERATOR="bt"

ARGS="-l ${TARGET_1} -r ${TARGET_2} -o ${OPERATOR}"
OUTPUT_FILE="logs/operator/OP_bt.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

# figure 16
# x = a uniform
# 32 bit
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for _ in {1..1}; do
    TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
    TARGETS+=${TARGET}
    TARGETS+=","
done
TARGET_1=${TARGETS}

OPERATOR="eq"

ARGS="-l ${TARGET_1} -o ${OPERATOR}"
OUTPUT_FILE="logs/operator/OP_eq_uniform.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

# figure 16
# x = a skewed
# 32 bit
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"

TARGET_1=0

OPERATOR="eq"

ARGS="-l ${TARGET_1} -o ${OPERATOR} -f ${ZIPF_2_PATH}"
OUTPUT_FILE="logs/operator/OP_eq_skewed.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi
