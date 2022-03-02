#!/bin/bash

mkdir -p logs/operator/

DATA_FILE=f10_data.txt
rm -f ${DATA_FILE}
cat f5a_data.txt | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}

WIDTH=32
MAX_VAL=$[1<<${WIDTH}]
PROGRAM="numactl --membind=0 ./bindex"

# create log dir

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


cat ${OUTPUT_FILE} | grep "bt" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}

OPERATOR="eq"
ARGS="-l 2333333 -o ${OPERATOR}"
OUTPUT_FILE="logs/operator/OP_eq_uni.log"
echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi
cat ${OUTPUT_FILE} | grep "eq" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}


OPERATOR="eq"
ARGS="-l 1 -o ${OPERATOR} -f ../data/zipf_2_32_1e9.data"
OUTPUT_FILE="logs/operator/OP_eq_zipf.log"
echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi
cat ${OUTPUT_FILE} | grep "eq" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${DATA_FILE}
