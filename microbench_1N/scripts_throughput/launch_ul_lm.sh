#!/bin/bash
#
# Args:
#  $1: nb consumers
#  $2: message size in B
#  $3: duration of the experiment in seconds
#  $4: max nb of messages in the circular buffer


# get arguments
if [ $# -eq 4 ]; then
   NB_CONSUMERS=$1
   MSG_SIZE=$2
   DURATION_XP=$3
   MAX_NB_MSG=$4
else
   echo "Usage: ./$(basename $0) <nb_consumers> <message_size_in_B> <xp_duration_in_sec> <max_nb_messages_in_circular_buffer>"
   exit 0
fi

OUTPUT_DIR="microbench_ul_lm_${NB_CONSUMERS}consumers_${DURATION_XP}sec_${MSG_SIZE}B_${MAX_NB_MSG}messages_in_buffer"

if [ -d $OUTPUT_DIR ]; then
   echo UL LM ${NB_CONSUMERS} consumers, ${DURATION_XP} sec, ${MSG_SIZE}B ${MAX_NB_MSG} msg in channel already done
   exit 0
fi

./stop_all.sh
./remove_shared_segment.pl

# used by ftok
touch /tmp/ul_lm_0copy_microbenchmark

# modify shared mem parameters
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmall
sudo ./root_set_value.sh 16000000000 /proc/sys/kernel/shmmax

# recompile with message size
echo "-DCOMPUTE_CYCLES -DNB_MESSAGES=$MAX_NB_MSG -DMESSAGE_MAX_SIZE=$MSG_SIZE" > UL_LM_0COPY_PROPERTIES
#echo "-DCOMPUTE_CYCLES -DNOP -DNB_MESSAGES=$MAX_NB_MSG -DMESSAGE_MAX_SIZE=$MSG_SIZE" > UL_LM_0COPY_PROPERTIES
#echo "-DCOMPUTE_CYCLES -DUSLEEP -DNB_MESSAGES=$MAX_NB_MSG -DMESSAGE_MAX_SIZE=$MSG_SIZE" > UL_LM_0COPY_PROPERTIES
make ul_lm_0copy_microbench

# launch XP
./bin/ul_lm_0copy_microbench -r $NB_CONSUMERS -s $MSG_SIZE -t $DURATION_XP

./stop_all.sh
./remove_shared_segment.pl

# save files
mkdir $OUTPUT_DIR
mv statistics*.log $OUTPUT_DIR/
