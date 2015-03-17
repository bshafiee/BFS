#!/bin/bash
function printHelp {
    echo -e "Use as: ./analyze.sh\n\t-i Input Directory\n\t-o Output Directory"
}

inputDir=""
outputDir=""

while [[ $# > 1 ]]
do
    key="$1"

    case $key in
        -i)
            inputDir="$2"
        shift
        ;;
        -o)
            outputDir="$2"
        shift
        ;;
        *)
            printHelp
            exit
        ;;
    esac
    shift
done

if [ -z "$inputDir" ] || [ -z "$outputDir" ] ; then
    printHelp
    exit
fi

if [ ! -d $inputDir ]; then
    echo $inputDir does not exist.
    exit
fi

if [ ! -d $outputDir ]; then
    echo "$outputDir does not exist, creating it..."
    mkdir -p $outputDir
fi

#processing files
for size in $(ls $inputDir); do
    echo Processing $size
    resDir=$outputDir/$size
    #replace // with / in the output dir
    resDir=`echo $resDir|sed 's/\/\//\//g'`
    echo $resDir
    ##mkdir -p $resDir
    echo "AggrWrite(KB/Sec),AggrRead(Kb/Sec)" >$resDir
    for itr in $(ls $inputDir/$size); do
        readersTotal=`grep -o "Children see throughput for [[:digit:]]\+[[:space:]]*readers[[:space:]]*=[[:space:]]*[[:digit:]]*\.\?[[:digit:]]*[[:space:]]*kB/sec" $inputDir/$size/$itr|sed 's/Children see throughput for [[:digit:]]\+[[:space:]]*readers[[:space:]]*=[[:space:]]*\([[:digit:]]*\.\?[[:digit:]]*\)[[:space:]]*kB\/sec/\1/'`
        writersTotal=`grep -o "Children see throughput for [[:digit:]]\+[[:space:]]*initial writers[[:space:]]*=[[:space:]]*[[:digit:]]*\.\?[[:digit:]]*[[:space:]]*kB/sec" $inputDir/$size/$itr|sed 's/Children see throughput for [[:digit:]]\+[[:space:]]*initial writers[[:space:]]*=[[:space:]]*\([[:digit:]]*\.\?[[:digit:]]*\)[[:space:]]*kB\/sec/\1/'`
        echo "$writersTotal,$readersTotal" >>$resDir
    done
done
