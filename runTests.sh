#!/bin/bash
inputdir="${1}"
outputdir="${2}"
numthreads="${3}"
numbuckets="${4}"

[[ -d $outputdir ]] || mkdir ${outputdir}
for test in $(ls ${inputdir}/*.*)
do
    testname=${test#*/}
    threadArg=$((numthreads/${numthreads#-}))
    bucketsArg=$((numbuckets/${numbuckets#-}))
    echo InputFile=${test#*/} NumThreads=${threadArg}
    ./tecnicofs-nosync ${inputdir}/${testname} ${outputdir}/${testname%%.*}-${threadArg}.txt ${threadArg} ${numbuckets} | grep -h "TecnicoFS completed in "
    for threads in $(seq 2 $((numthreads*threadArg)))
    do
        echo InputFile=${test#*/} NumThreads=$((threads*threadArg))
        ./tecnicofs-mutex ${inputdir}/${testname} ${outputdir}/${testname%%.*}-${threads}.txt $((threads*threadArg)) ${numbuckets} | grep -h "TecnicoFS completed in "
    done
done
