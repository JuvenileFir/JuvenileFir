#!/bin/bash

WIDTH=32
PROGRAM="numactl --membind=0 ./column-sketches.out"
ZIPF_1_PATH=../data/zipf_1_32_1e9.data
ZIPF_2_PATH=../data/zipf_2_32_1e9.data

# create log dir
rm -rf logs/zipf
mkdir -p logs/zipf

OPERATOR="lt"
CODE_WIDTH="WIDTH_${WIDTH}"

# 1
# selectivity 0 -> 1, 0.1
# zipf = 1
# 32 bit
# MAX_VAL=$[1<<${WIDTH}]
# TARGETS=""
# for SELECTIVITY in {0..100..10}; do
#         TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
#         TARGETS+=${TARGET}
#         TARGETS+=","
# done
# ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_1_PATH}"
# OUTPUT_FILE="logs/zipf/ZIPF_1.log"

# echo ${ARGS}
# ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

# if [ $? -ne 0 ]; then
# 	echo BAD!!!!${OUTPUT_FILE}
# fi

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for TARGET in 0 110 2126821268 3503591 183839380 734131076 1393095833 2103845272 2826687846 3555619953 $[${MAX_VAL} - 2]; do
        TARGETS+=${TARGET}
        TARGETS+=","
done
ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_1_PATH}"
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
# MAX_VAL=$[1<<${WIDTH}]
# TARGETS=""
# for SELECTIVITY in {0..100..10}; do
#         TARGET=$[(${MAX_VAL} - 2) * ${SELECTIVITY} / 100]
#         TARGETS+=${TARGET}
#         TARGETS+=","
# done
# ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_2_PATH}"
# OUTPUT_FILE="logs/zipf/ZIPF_2.log"

# echo ${ARGS}
# ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

# if [ $? -ne 0 ]; then
# 	echo BAD!!!!${OUTPUT_FILE}
# fi

MAX_VAL=$[1<<${WIDTH}]
TARGETS=""
for TARGET in 0 0 0 0 0 0 0 1 2 5 $[${MAX_VAL} - 2]; do
        TARGETS+=${TARGET}
        TARGETS+=","
done
ARGS="-l ${TARGETS} -o ${OPERATOR} -f ${ZIPF_2_PATH}"
OUTPUT_FILE="logs/zipf/ZIPF_2.log"

echo ${ARGS}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

if [ $? -ne 0 ]; then
	echo BAD!!!!${OUTPUT_FILE}
fi
