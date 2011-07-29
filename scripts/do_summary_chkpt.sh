#!/bin/bash
#
# Make a summary of the experiments.
#  $1: directory where to find the results
#  $2: output file

# This script is mostly result filename agnostic.
# However, you cannot mix different communication mechanism: this is not taken into account

function print_help_and_exit {
echo "Usage: ./$(basename $0) <results_directory> <output_file>"
exit 0
}

# The main function
function do_summary {
echo -e "#nb_nodes\tmsg_size\tchkpt_size\tnb_iter\tchannel_size\tlimit_thr\tthroughput\tlatency" > $OUTPUT_FILE

for file in ${RESULTS_DIR}/*.txt; do
   # extract the values from the filename
   grep channelSize <<< $file &> /dev/null
   if [ $? -eq 0 ]; then
      str=$(sed 's/^\(.\+\)_\([[:digit:]]\+\)nodes_\([[:digit:]]\+\)iter_chkpt\([[:digit:]]\+\)_msg\([[:digit:]]\+\)B_\([[:digit:]]\+\)channelSize.txt/\1\t\2\t\3\t\4\t\5\t\6/' <<< $file)

      chan_size=$(awk '{print $6}' <<< $str)
      limit_thr=0
   else
      grep snapPerSec <<< $file &> /dev/null

      if [ $? -eq 0 ]; then
         str=$(sed 's/^\(.\+\)_\([[:digit:]]\+\)nodes_\([[:digit:]]\+\)iter_chkpt\([[:digit:]]\+\)_msg\([[:digit:]]\+\)B_thr\([[:digit:]]\+\)snapPerSec.txt/\1\t\2\t\3\t\4\t\5\t\6/' <<< $file)

         limit_thr=$(awk '{print $6}' <<< $str)
      else
         str=$(sed 's/^\(.\+\)_\([[:digit:]]\+\)nodes_\([[:digit:]]\+\)iter_chkpt\([[:digit:]]\+\)_msg\([[:digit:]]\+\)B.txt/\1\t\2\t\3\t\4\t\5/' <<< $file)

         limit_thr=0
      fi

      chan_size=0
   fi

   comm_mech=$(awk '{print $1}' <<< $str)
   nb_nodes=$(awk '{print $2}' <<< $str)
   nb_iter=$(awk '{print $3}' <<< $str)
   chkpt_size=$(awk '{print $4}' <<< $str)
   msg_size=$(awk '{print $5}' <<< $str)

   throughput=$(grep thr $file | awk '{print $6}')
   if [ -z $throughput ]; then
      continue
   fi

   latency=$(echo "1 / $throughput * 1000 * 1000" | bc -l)

   echo -e "$nb_nodes\t$msg_size\t$chkpt_size\t$nb_iter\t$chan_size\t$limit_thr\t$throughput\t$latency" >> $OUTPUT_FILE
done

# sort the file
tmp=$(mktemp chkpt_sort_XXX)

sort -n -k 1 -k 2 -k 3 -k 4 -k 5 -k 6 ${OUTPUT_FILE} > $tmp
mv $tmp ${OUTPUT_FILE}
}

if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   print_help_and_exit  
fi

do_summary
