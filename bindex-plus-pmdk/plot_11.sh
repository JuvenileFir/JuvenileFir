#!/bin/bash

REFINE_DATA_FILE=f11_data_refine.txt
COPY_DATA_FILE=f11_data_copy.txt
rm -f ${REFINE_DATA_FILE} ${COPY_DATA_FILE}

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
for SELECTIVITY in {0..100..1}; do
    TARGET=$[(${MAX_VAL} - 1) * ${SELECTIVITY} / 100]
    TARGETS+=${TARGET}
    TARGETS+=","
done
OPERATOR="lt"

rm -rf logs/v_ps
mkdir -p logs/v_ps
OUTPUT_FILE="logs/v_ps/ps"


for PS in {0..8..1}; do
    ARGS="-l ${TARGETS} -o ${OPERATOR} -p ${PS}"
    echo ${ARGS}
    ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}_${PS}
    cat ${OUTPUT_FILE}_${PS} | grep "refine" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${REFINE_DATA_FILE}
    cat ${OUTPUT_FILE}_${PS} | grep "copy" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}' >> ${COPY_DATA_FILE}
done


echo "The copy/refine time will fluctuate, this script should run many times. It is needed to manually recording the average copy/refine time under different prefetch stride."
if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

