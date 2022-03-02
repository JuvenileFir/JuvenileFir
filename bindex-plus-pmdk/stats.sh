#!/bin/bash

get_exe_time () {
    cat $1 | grep $2 | grep -oP "[\d.]+(?= ms)" | awk 'NR > 1 {total += $1} END {print total/(NR-1)}'
}

pick_time () {
    cat $1 | grep -oP "(?<=Wall time \(ms\): )[\d.]+"
}
