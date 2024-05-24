#!/bin/sh

output_prefix="trace__"
cmd=$@
export PROFILER_OPTS="OUTPUT_FILE_PREFIX=$output_prefix,TRACE_INTERVAL_US=600000,ADDR2LINE_PATH=`which addr2line`"
SCRIPT_PATH=$(realpath `which $0`)
SCRIPT_DIR=`dirname $SCRIPT_PATH`
echo "Executing $cmd"
#${cmd[@]}
export LD_PRELOAD="$SCRIPT_DIR/profiler.so"
eval $cmd
