#!/bin/bash
#
# Util functions and variables for making a summary of the experiments


XP_DURATION=$((5*60))   # 5 minutes
NUM_CONSUMERS_ARRAY=( 1 3 5 7 )
MSG_SIZE_ARRAY=( 64 1024 10240 102400 1048576 )


# Create the summary
# This function calls get_dir_name with 3 args: num_consumersm xp_duration and msg_size
#  $1: results directory
#  $2: output file
function do_summary1 {
if [ $# -eq 2 ]; then
   RESULTS_DIR="$1"
   OUTPUT_FILE=$2
else
   echo "Usage: ./$(basename $0) <results_directory> <output_file>"
   return -1
fi


for num_consumers in ${NUM_CONSUMERS_ARRAY[@]}; do
   for i in $(seq 0 $(( ${#MSG_SIZE_ARRAY[@]}-1 )) ); do

      msg_size=${MSG_SIZE_ARRAY[$i]}
      dir=$(get_dir_name $num_consumers $XP_DURATION $msg_size)
      nb_messages=$(grep nb_messages $RESULTS_DIR/$dir/statistics_producer.log | awk '{print $2}')
      thr_producer=$(grep thr $RESULTS_DIR/$dir/statistics_producer.log | awk '{print $2}')
      nb_cycles_send=$(grep nb_cycles_send $RESULTS_DIR/$dir/statistics_producer.log | awk '{print $2}')

      nb_cycles_recv=0
      thr_consumer=0

      for j in $(seq 1 $num_consumers); do
         thr=$(grep thr $RESULTS_DIR/$dir/statistics_consumer_${j}.log | awk '{print $2}')
         nb_cycles=$(grep nb_cycles_recv $RESULTS_DIR/$dir/statistics_consumer_${j}.log | awk '{print $2}')

         thr_consumer=$(echo "$thr_consumer + $thr" | bc -l)
         nb_cycles_recv=$(echo "$nb_cycles_recv + $nb_cycles" | bc -l)
      done

      thr_consumer=$(echo "$thr_consumer / $num_consumers" | bc -l)
      nb_cycles_recv=$(echo "$nb_cycles_recv / $num_consumers" | bc -l)

      echo -e "$num_consumers\t$msg_size\t$nb_messages\t$thr_producer\t$thr_consumer\t$nb_cycles_send\t$nb_cycles_recv" >> $OUTPUT_FILE

   done
done
}
