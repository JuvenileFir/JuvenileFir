#!/bin/bash

DATA_FILE=f12_data.txt
rm -f ${DATA_FILE}

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
g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D ${CODE_WIDTH} -o bindex -g

OPERATOR="lt"

ARGS="-l 2333333 -o ${OPERATOR} -i"
echo ${ARGS}
OUTPUT_FILE=/tmp/building.log
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}
cat ${OUTPUT_FILE} | grep "building" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR/1000}' >> ${DATA_FILE}
cat ${OUTPUT_FILE} | grep "APPEND 0.1 time" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR/1000}' >> ${DATA_FILE}
cat ${OUTPUT_FILE} | grep "APPEND 1 time" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR/1000}' >> ${DATA_FILE}
cat ${OUTPUT_FILE} | grep "APPEND 5 time" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR/1000}' >> ${DATA_FILE}
cat ${OUTPUT_FILE} | grep "APPEND 10 time" | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR/1000}' >> ${DATA_FILE}


if [ $? -ne 0 ]; then
    echo BAD!!!!${OUTPUT_FILE}
fi

