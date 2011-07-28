if [ $# -ne 1 ]; then
   echo "Need the communication mechanism as the first argument"
   exit 0
fi

COMM_MECH=$1
BENCH=microbench_${COMM_MECH}_$(hostname)

ping -c 1 frennes
if [ $? -eq 0 ]; then
   FRONTEND=rennes
else
   ping -c 1 freims
   if [ $? -eq 0 ]; then
      FRONTEND=reims
   else
      echo "Frontend is unknown"
   fi
fi

i=1
while [ true ]; do
 ./launch_xp_bfish_mprotect.sh

 DIR=$BENCH/run$i
 mkdir -p $DIR
 mv microbench* $DIR/

 AR=${BENCH}_run$i.tar.gz
 tar cfz $AR $DIR

 scp $AR $FRONTEND:

 i=$(($i+1))
done

