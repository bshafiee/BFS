#!/bin/bash

function PrintHelp {
echo -e "Usage: ./iozone.sh \n-z,--iozone\n\tPath to iozone binary\n-i,--iteration\n\t#Iterations per size (DEFAULT 3)\n-o,--output\n\tOutputDirectory (Default ~/resultIozone/\`hostname\`)\n-s,--size\n\tFileSize1(100M) -s FileSize2(1G) ...\n-l,--latency true\n\tEnables generating Latency files.\n-t,--thread\n\tNumber of processes(threads) for Iozone.\n-r,--remote\n\tPath to file containing remote nodes info."
}

function getKB() {
    num=`echo $1|sed 's/\([0-9]*\)\([[:alpha:]]*\)/\1/'`
    unit=`echo $1|sed 's/\([0-9]*\)\([[:alpha:]]*\)/\2/'`
    case "$unit" in
        M|m)
            echo $(($num*1024));;
        K|k)
            echo $num;;
        G|g)
            echo $(($num*1024*1024));;
        * )
            echo Invalid Unit!
            exit
    esac
}

iozone=""
nodelistfile=""
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
        -r|--remote)
            nodelistfile="$2"
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

if [ -z "$iozone" ] || [ -z "$outputdir" ] || [ -z "$nodelistfile" ] || [ -z "$thread" ] || [ -z "$iteration" ] || [ ${#sizeArr[@]} == 0 ]; then
    PrintHelp
    exit
fi

echo IOZONE PATH = "${iozone}"
echo Output Director = "${outputdir}"
echo Iterations = "${iteration}"
echo Sizes = ${sizeArr[*]}
echo Generate Lantency = $latency
echo Number Of Threads = $thread
echo Remote node path = $nodelistfile

#Check for output directory
if [ ! -d "$outputdir" ]; then
    echo -e "$outputdir Does not exist. Creating $outputdir...\n"
    mkdir -p $outpudir
fi

#Check record size
recSize="16M"
recSizeKB=$(getKB $recSize)


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
        recSize="16M"
        sizeKB=$(getKB $size)
        if [ $sizeKB -lt $recSizeKB ];then
            recSize=$size
        fi
        finalCommand="$command -c -e -i 0 -i 1 -+n -r $recSize -s $size -t $thread -+m $nodelistfile | tee $outputdir/$size/$COUNTER"
        echo Run: $COUNTER Size:$size : "$finalCommand"
        echo Sleeping: 5 seconds
        sleep 5
        eval $finalCommand
        let COUNTER=COUNTER+1
    done
done










