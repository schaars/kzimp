#!/bin/bash
#
# Make a summary of UDP experiments
#  $1: directory where to find the results
#  $2: output file


source summary_util.sh

# Return the name of the directory which contains the
# results for one experiment
#  $1: nb_consumers
#  $2: experiment duration in seconds
#  $3: requests size
function get_dir_name {
echo "microbench_inet_udp_${1}consumers_${2}sec_${3}B"
}


if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   echo "Usage: ./$(basename $0) <results_directory> <output_file>"
   exit 0
fi

do_summary1 $RESULTS_DIR $OUTPUT_FILE

