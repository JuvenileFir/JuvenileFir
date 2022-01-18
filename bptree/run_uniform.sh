#!/bin/bash

source ./stats.sh
PROGRAM="numactl --membind=0 ./simple_bpt"

g++ -std=c++11 simple_bptree.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -o simple_bpt

# create log dir
LOG_DIR="logs"
RAW_DATA_DIR=${LOG_DIR}/raw_data
RESULT_FILE=${LOG_DIR}/result.txt
SELECTIVITY_FILE=${LOG_DIR}/selectivities.txt


rm -rf ${LOG_DIR}
mkdir -p ${LOG_DIR}

mkdir -p ${RAW_DATA_DIR}

OUTPUT_FILE="${RAW_DATA_DIR}/result"
echo $OUTPUT_FILE

NUM_SELECTIVITIES=15
for i in `seq 1 1 ${NUM_SELECTIVITIES}`
do
    factor=$((2 ** i))
    s[i]=`awk "BEGIN {print 1e-5*$factor}"`
    echo ${s[i]}
    echo ${s[i]} >> ${SELECTIVITY_FILE}
done
selectivities=$(IFS=$','; echo "${s[*]}")
echo $selectivities

ARGS="-s $selectivities -r 10 -o lt"

# Run

${PROGRAM} ${ARGS} > ${OUTPUT_FILE}

get_exe_time ${OUTPUT_FILE} "BUILDING" | awk '{print $1/1000}' > ../palmtree/f12_data.txt

# Split outputs to multiple files
awk -v filename=${OUTPUT_FILE} 'NR > 1 {print "RUNNING " $0 > filename NR-1}' RS="RUNNING" ${OUTPUT_FILE}
for i in `seq 1 1 ${NUM_SELECTIVITIES}`
do
    get_exe_time "${OUTPUT_FILE}$((${i}))" "ms" >> ${RESULT_FILE}
done

cp ${RESULT_FILE} f7_data.txt
cp ${SELECTIVITY_FILE} f7_selectivity.txt 
