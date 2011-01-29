#!/bin/bash
#
# Get and save current memory usage
# The script lasts until it is killed
# Args:
#   $1: output dir


if [ $# -eq 1 ]; then
   MEMORY_DIR=$1
else
   echo "Usage: ./$(basename $0) <output_dir>"
   exit 0
fi


while [ 1 ]; do
   # get number of usec since the epoch
   current_time=$(( $(date +%s) * 1000000 ))

   vmstat -m > $MEMORY_DIR/vmstat_${current_time}.log
   cat /proc/meminfo > $MEMORY_DIR/meminfo_${current_time}.log
   sleep 1
done
