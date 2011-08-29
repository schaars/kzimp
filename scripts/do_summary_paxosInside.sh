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
echo -e "#nb_paxos_nodes\tnb_clients\treq_size\tnb_iter\tleader_acceptor\tchannel_size\tthroughput" > $OUTPUT_FILE

for file in ${RESULTS_DIR}/*.txt; do
   # extract the values from the filename
   if [[ "$file" =~ "channelSize" ]]; then
      str=$(sed 's/^\(.\+\)_\([[:digit:]]\+\)nodes_\([[:digit:]]\+\)clients_\([[:digit:]]\+\)iter_\([[:digit:]]\+\)B_\(.\+\)_\([[:digit:]]\+\)channelSize.txt/\1\t\2\t\3\t\4\t\5\t\6\t\7/' <<< $file)

      chan_size=$(awk '{print $7}' <<< $str)
   else
      str=$(sed 's/^\(.\+\)_\([[:digit:]]\+\)nodes_\([[:digit:]]\+\)clients_\([[:digit:]]\+\)iter_\([[:digit:]]\+\)B_\(.\+\).txt/\1\t\2\t\3\t\4\t\5\t\6/' <<< $file)

      chan_size=0
   fi

   comm_mech=$(awk '{print $1}' <<< $str)
   nb_nodes=$(awk '{print $2}' <<< $str)
   nb_clients=$(awk '{print $3}' <<< $str)
   nb_iter=$(awk '{print $4}' <<< $str)
   req_size=$(awk '{print $5}' <<< $str)
   leader_acceptor=$(awk '{print $6}' <<< $str)

   throughput=$(grep thr $file | awk '{print $4}')

   echo -e "$nb_nodes\t$nb_clients\t$req_size\t$nb_iter\t$leader_acceptor\t$chan_size\t$throughput" >> $OUTPUT_FILE
done

# sort the file
tmp=$(mktemp paxosInside_sort_XXX)

sort -n -k 1 -k 2 -k 3 -k 4 -k 6 ${OUTPUT_FILE} > $tmp
mv $tmp ${OUTPUT_FILE}
}

if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   print_help_and_exit  
fi

do_summary
