#!/bin/bash

source ./stats.sh
PROGRAM="numactl --membind=0 ./bindex"

DIR="logs/compare_with_bpt"
RAW_DATA_DIR=${DIR}/raw_data

RESULT_FILE=${DIR}/result.txt
SELECTIVITY_FILE=${DIR}/selectivities.txt
OUTPUT_FILE=${RAW_DATA_DIR}/result

rm -rf ${DIR}
mkdir -p ${DIR}
mkdir -p ${RAW_DATA_DIR}

SELECTIVITY_FILE=${DIR}/selectivities.txt

WIDTH=32
MAX_VAL=$[1<<${WIDTH}]
NUM_SELECTIVITIES=10
for i in `seq 0 1 ${NUM_SELECTIVITIES}`
do
    factor=$((2 ** i))
    s[i]=`awk "BEGIN {print 1e-5*$factor}"`
    echo ${s[i]}
    echo ${s[i]} >> ${SELECTIVITY_FILE}
    targets[i]=`awk "BEGIN {print ${s[i]}*${MAX_VAL}}"`
    echo ${targets[i]}
done
selectivities=$(IFS=$','; echo "${s[*]}")
TARGETS=$(IFS=$','; echo "${targets[*]}")

echo $TARGETS


# Run
OPERATOR="lt"
ARGS="-l ${TARGETS} -o ${OPERATOR}"
g++ bindex.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -D DATA_N=1e9 -o bindex
# numactl --membind=0 ${PROGRAM} ${ARGS} > ${OUTPUT_FILE}
${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

# Split outputs to multiple files
awk -v filename=${OUTPUT_FILE} 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${OUTPUT_FILE}
for i in `seq 0 1 ${NUM_SELECTIVITIES}`
do
    get_exe_time "${OUTPUT_FILE}$((${i}+1))" "lt" >> ${RESULT_FILE}
done

cp ${RESULT_FILE} f7_data.txt
cp ${SELECTIVITY_FILE} f7_selectivity.txt
