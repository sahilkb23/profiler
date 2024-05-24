#!/bin/sh

output_prefix="trace__"
export PROFILER_OPTS="OUTPUT_FILE_PREFIX=$output_prefix,TRACE_INTERVAL_US=300000,ADDR2LINE_PATH=`which addr2line`"
SCRIPT_PATH=$(realpath `which $0`)
SCRIPT_DIR=`dirname $SCRIPT_PATH`
PID=$1
#${cmd[@]}
gdb_script_file="$PID.gdbscript"
gdb_out_file="$PID.gdbout"
rm -rf $gdb_script_file
touch $gdb_script_file
gdb_file_contents=$(cat <<EOF
call (void)dlopen("$SCRIPT_DIR/profiler.so", 2)
q
EOF
        ) 
echo "$gdb_file_contents" > $gdb_script_file
echo "Attaching to $PID"
(gdb -p $PID -x $gdb_script_file > $gdb_out_file) && echo "Attached to $PID" || echo "Fail to attach to PID $PID"
rm -rf $gdb_script_file
