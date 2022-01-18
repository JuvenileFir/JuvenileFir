#!/bin/bash

get_exe_time () {
    cat $1 | grep $2 | grep -oP "[\d.]+(?= ms)" | awk '{total += $1} END {print total/NR}'
}

pick_time () {
    cat $1 | grep -oP "(?<=Wall time \(ms\): )[\d.]+"
}
