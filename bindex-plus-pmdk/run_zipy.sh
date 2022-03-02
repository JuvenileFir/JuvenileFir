#!/bin/bash
#python3 generate_zipf_data.py

WIDTH=32
PROGRAM="numactl --membind=0 ./bindex"
ZIPF_1_PATH=../data/zipf_1_32_1e9.data
ZIPF_2_PATH=../data/zipf_2_32_1e9.data

# create log dir
rm -rf logs/zipf
mkdir -p logs/zipf

OPERATOR="lt"
CODE_WIDTH="WIDTH_${WIDTH}"
g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -o bindex


# 1
# selectivity 0 -> 1, 0.1
# zipf = 1
# 32 bit
MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for SELECTIVITY in {0..100..10}; do
        TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
        TARGETS+=${TARGET}
        TARGETS+=","
done
ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_1_PATH} -s"
OUTPUT_FILE="logs/zipf/ZIPF_1.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
	echo BAD!!!!${OUTPUT_FILE}
fi

# 2
# selectivity 0 -> 1, 0.1
# zipf = 2
# 32 bit

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for SELECTIVITY in {0..100..10}; do
        TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
        TARGETS+=${TARGET}
        TARGETS+=","
done
ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_2_PATH} -s"
OUTPUT_FILE="logs/zipf/ZIPF_2.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
	echo BAD!!!!${OUTPUT_FILE}
fi
