#!/bin/bash
#
# Make a summary of the experiments for kzimp, microbench
# and also for bfish mprotect (microbench too)
#  $1: directory where to find the results
#  $2: output file


function print_help_and_exit {
echo "Usage: ./$(basename $0) <results_directory> <output_file>"
exit 0
}


# Create the summary for kzimp
#  $1: results directory
#  $2: output file
function do_summary_kzimp {
if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   echo "Usage: ./$(basename $0) <results_directory> <output_file>"
   return -1
fi

echo -e "num_consumers\tmsg_size\tnum_msg_channel\tthr_producer" > $OUTPUT_FILE

for dir in ${RESULTS_DIR}/*; do

   if [ ! -d $dir ]; then
      # this is not a directory
      continue
   fi

   # extract the values from the directory name
   str=$(echo "$dir" | sed 's/^.*microbench_kzimp_\([[:digit:]]\+\)consumers_\([[:digit:]]\+\)sec_\([[:digit:]]\+\)B_\([[:digit:]]\+\)messages_in_buffer.*/\1\t\2\t\3\t\4/')
   num_consumers=$(echo $str | awk '{print $1}')
   xp_duration=$(echo $str | awk '{print $2}')
   msg_size=$(echo $str | awk '{print $3}')
   num_msg_channel=$(echo $str | awk '{print $4}')

   thr_producer=$(grep thr $dir/statistics_producer.log | awk '{print $2}')

   echo -e "$num_consumers\t$msg_size\t$num_msg_channel\t$thr_producer" >> $OUTPUT_FILE

done

# sort the file
tmp=$(mktemp kzimp_sort_XXX)

sort -n -k 1 -k 2 -k 3 ${OUTPUT_FILE} > $tmp
mv $tmp ${OUTPUT_FILE}
}


# Create the summary for bfish_mprotect
#  $1: results directory
#  $2: output file
function do_summary_bfish_mprotect {
if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   echo "Usage: ./$(basename $0) <results_directory> <output_file>"
   return -1
fi

echo -e "num_consumers\tmsg_size\tnum_msg_channel\tthr_producer" > $OUTPUT_FILE

for dir in ${RESULTS_DIR}/*; do

   if [ ! -d $dir ]; then
      # this is not a directory
      continue
   fi

   # extract the values from the directory name
   str=$(echo "$dir" | sed 's/^.*microbench_bfish_mprotect_\([[:digit:]]\+\)consumers_\([[:digit:]]\+\)sec_\([[:digit:]]\+\)B_\([[:digit:]]\+\)messages_in_buffer.*/\1\t\2\t\3\t\4/')
   num_consumers=$(echo $str | awk '{print $1}')
   xp_duration=$(echo $str | awk '{print $2}')
   msg_size=$(echo $str | awk '{print $3}')
   num_msg_channel=$(echo $str | awk '{print $4}')

   thr_producer=$(grep thr $dir/statistics_producer.log | awk '{print $2}')

   echo -e "$num_consumers\t$msg_size\t$num_msg_channel\t$thr_producer" >> $OUTPUT_FILE

done

# sort the file
tmp=$(mktemp kzimp_sort_XXX)

sort -n -k 1 -k 2 -k 3 ${OUTPUT_FILE} > $tmp
mv $tmp ${OUTPUT_FILE}
}


if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   print_help_and_exit  
fi

#do_summary_kzimp $RESULTS_DIR $OUTPUT_FILE
do_summary_bfish_mprotect $RESULTS_DIR $OUTPUT_FILE
