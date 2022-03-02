#!/bin/bash

python3 generate_zipf_data.py

WIDTH=32
MAX_VAL=$[1<<${WIDTH}]
PROGRAM="numactl --membind=0 ./bindex"

# create log dir
rm -rf logs/operator
mkdir -p logs/operator

# figure 14
# x > a
# selectivity 0:100:10
# 32 bit
# TODO: MAX_VAL - 2
WIDTH=32
CODE_WIDTH="WIDTH_${WIDTH}"
g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -o bindex

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for SELECTIVITY in {0..100..10}; do
    TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
    TARGETS+=${TARGET}
    TARGETS+=","
done

OPERATOR="gt"

ARGS="-l ${TARGETS} -o ${OPERATOR} -s"
OUTPUT_FILE="logs/operator/OP_gt.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

# figure 15, 16
# x < a
# 32 bit
# WIDTH=32
# CODE_WIDTH="WIDTH_${WIDTH}"
# g++ hydex_scan.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH}

# MAX_VAL=$[1<<${WIDTH}]
# TARGETS=""
# for _ in {1..100}; do
#     TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
#     TARGETS+=${TARGET}
#     TARGETS+=","
# done
# TARGET_1=${TARGETS}

# OPERATOR="lt"

# ARGS="-l ${TARGET_1} -o ${OPERATOR}"
# OUTPUT_FILE="logs/operator/OP_lt.log"

# echo ${ARGS}
# ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

# if [ $? -ne 0 ]; then
#     echo BAD!!!!${OUTPUT_FILE}
# fi

# # figure 15, 16
# # a < x < b
# # 32 bit
# WIDTH=32
# CODE_WIDTH="WIDTH_${WIDTH}"
# g++ hydex_scan.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH}

# MAX_VAL=$[1<<${WIDTH}]
# TARGETS=""
# for _ in {1..100}; do
#     TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
#     TARGETS+=${TARGET}
#     TARGETS+=","
# done
# TARGET_1=${TARGETS}
# TARGETS=""
# for _ in {1..100}; do
#     TARGET=$(shuf -i 0-${MAX_VAL} -n 1)
#     TARGETS+=${TARGET}
#     TARGETS+=","
# done
# TARGET_2=${TARGETS}

# OPERATOR="bt"

# ARGS="-l ${TARGET_1} -r ${TARGET_2} -o ${OPERATOR}"
# OUTPUT_FILE="logs/operator/OP_bt.log"

# echo ${ARGS}
# ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

# if [ $? -ne 0 ]; then
#     echo BAD!!!!${OUTPUT_FILE}
# fi
