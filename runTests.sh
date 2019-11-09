#!/bin/bash
inputdir="${1}"
outputdir="${2}"
numthreads="${3}"
numbuckets="${4}"

[[ -d $outputdir ]] || mkdir ${outputdir}
for test in $(ls ${inputdir}/*.*)
do
    testname=${test#*/}
    echo InputFile=${test#*/} NumThreads=1
    ./tecnicofs-nosync ${inputdir}/${testname} ${outputdir}/${testname%%.*}-1.txt 1 1 | grep -h "TecnicoFS completed in "
    for threads in $(seq 2 ${numthreads})
    do
        echo InputFile=${test#*/} NumThreads=${threads}
        ./tecnicofs-mutex ${inputdir}/${testname} ${outputdir}/${testname%%.*}-${threads}.txt ${threads} ${numbuckets} | grep -h "TecnicoFS completed in "
    done
done
