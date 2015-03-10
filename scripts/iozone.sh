#!/bin/bash

function PrintHelp {
echo -e "Usage: ./iozone.sh \n-z,--iozone\n\tPath to iozone binary\n-i,--iteration\n\t#Iterations per size (DEFAULT 3)\n-o,--output\n\tOutputDirectory (Default ~/resultIozone/\`hostname\`)\n-s,--size\n\tFileSize1(100M) -s FileSize2(1G) ...\n-l,--latency true\n\tEnables generating Latency files.\n-t,--thread\n\tNumber of processes(threads) for Iozone."
}

iozone=""
outputdir="~/resultIozone/`hostname`"
iteration=-1
sizeArr=()
latency=false
thread=""


while [[ $# > 1 ]]
do
    key="$1"

    case $key in
        -z|--iozone)
            iozone="$2"
        shift
        ;;
        -i|--iteration)
            iteration="$2"
        shift
        ;;
        -s|--size)
            sizeArr+=("$2")
        shift
        ;;
        -o|--output)
            outputdir="$2"
        shift
        ;;
        -l|--latency)
            latency=true
        shift
        ;;
        -t|--thread)
            thread="$2"
        shift
        ;;
        --default)
            PrintHelp
        shift
        ;;
        *)
            PrintHelp
            exit
        ;;
    esac
    shift
done

if [ -z "$iozone" ] || [ -z "$outputdir" ] || [ -z "$thread" ] || [ -z "$iteration" ] || [ ${#sizeArr[@]} == 0 ]; then
    PrintHelp
    exit
fi

echo IOZONE PATH = "${iozone}"
echo Output Director = "${outputdir}"
echo Iterations = "${iteration}"
echo Sizes = ${sizeArr[*]}
echo Generate Lantency = $latency
echo Number Of Threads = $thread

#Check for output directory
if [ ! -d "$outputdir" ]; then
    echo -e "$outputdir Does not exist. Creating $outputdir...\n"
    mkdir -p $outpudir
fi

#create time-directory in the output dir
outputdir="$outputdir/`date +\"%d-%b-%Y-%T\"`"
mkdir -p "$outputdir"

for size in "${sizeArr[@]}";
do
    command="$iozone"
    if [ "$latency" = true ];then
        command+=" -Q"
    fi
    COUNTER=1
    mkdir -p $outputdir/$size
    while [ $COUNTER -le $iteration ]; do
        finalCommand="$command -c -e -i 0 -i 1 -+n -r 16M -s $size -t $thread | tee $outputdir/$size/$COUNTER"
        eval $finalCommand
        echo Run: $COUNTER Size:$size : "$finalCommand"
        let COUNTER=COUNTER+1
    done
done










