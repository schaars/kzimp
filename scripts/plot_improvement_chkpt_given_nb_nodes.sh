#!/bin/bash
#
# Plot a figure with gnuplot from checkpointing mean/stdev summaries,
# for a given number of nodes, about the improvement
#
# Args:
#  $1: out file
#  $2: lat or thr?
#  $3: given nb nodes
# The next arguments are optionnal and must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file


if [ -z $LOG_SCALE ]; then
   LOG_SCALE=0
fi

# Args:
#  $1: PLOT_FILE
#  $2: XLABEL
#  $3: YLABEL
#  $4: TITLE
function complete_header {
PLOT_FILE=$1
XLABEL=$2
YLABEL=$3
TITLE=$4

# do we use log scale or not?
if [ $LOG_SCALE -eq 1 ]; then
   YLABEL="${YLABEL} (log scale)"
fi

cat << EOF > $PLOT_FILE
set term postscript eps
set output "${OUT_FILE}.eps"

set xlabel "${XLABEL}"
set ylabel "${YLABEL}"
#set title "$TITLE"

set logscale x
set xtics("1B" 1,"64B" 64,"128B" 128,"512B" 512,"1kB" 1024,"4kB" 4096,"10kB" 10240,"100kB" 102400,"1MB" 1048576)

#set key left top
#set key at 3.35,5300

set xrange [128:]

EOF

if [ $LOG_SCALE -eq 1 ]; then
   echo "set logscale y" >> $PLOT_FILE
   echo "set yrange [1:]" >> $PLOT_FILE
else
   # The improvement may be < 0
   #echo "set yrange [0:]" >> $PLOT_FILE
   echo "set yrange [:]" >> $PLOT_FILE
fi
}

 
# extract data that will be plotted
function extract_data {
file=$1
nb_nodes=$2

# empty the output file
out=${file}.data
> $out

while read line; do
   grep "^${nb_nodes}[[:space:]]" <<< $line &> /dev/null
   if [ $? -eq 0 ]; then
      msg_size=$(awk '{print $2}' <<< $line)

      if [ $LAT_OR_THR == "lat" ]; then
         y_value=$(awk '{print $10}' <<< $line)
         stddev=$(awk '{print $11}' <<< $line)
      elif [ $LAT_OR_THR == "thr" ]; then
         y_value=$(awk '{print $7}' <<< $line)
         stddev=$(awk '{print $8}' <<< $line)
      fi

      echo -e "${msg_size}\t${y_value}\t${stddev}" >> $out
   fi
done < $file
}

function get_improvement {
new_args=""

for arg in "$@"; do
   title="$1"; shift
   file=$1; shift

   if [ -z "$title" ] || [ -z $file ]; then
      break
   fi

   extract_data $file $NB_NODES

   # replace spaces by underscore in the title
   new_args="${new_args} "${title// /_}" ${file}.data"
done

# compute improvement using $new_args
$(dirname $0)/compute_improvement_chkpt.py ${LAT_OR_THR} ${new_args}
}

function complete_plot {
PLOT_FILE=$1
shift

# we do not have to plot the first summary, which is the reference
shift; shift

echo -n "plot " >> $PLOT_FILE

first=1
Y=2
S=3
for arg in "$@"; do
   title="$1"; shift
   file=$1; shift

   if [ -z "$title" ] || [ -z $file ]; then
      break
   fi

   if [ $first -eq 0 ]; then
      echo -n ", " >> $PLOT_FILE
   fi

   #echo -n \"plot.data\" using 1:$Y:$S title \""$title"\" with yerrorlines >> $PLOT_FILE
   echo -n \"plot.data\" using 1:$Y title \""$title"\" with linespoint >> $PLOT_FILE

   first=0
   Y=$(($Y+2))
   S=$(($S+2))
done

echo "" >> $PLOT_FILE
}


# Get data for a figure about latency
# OUT_FILE and NB_NODES must be defined
# $1 is the PLOT_FILE that will be given to gnuplot
# The other arguments refer to the titles and the summary files
function create_lat_data {
PLOT_FILE=$1
shift

# what are the labels?
XLABEL="Checkpoint size (log scale)"
YLABEL="Snapshot completion time improvement in %"
TITLE="Checkpointing, ${NB_NODES} nodes"

complete_header $PLOT_FILE "$XLABEL" "$YLABEL" "$TITLE"

# get results
get_improvement "$@"

complete_plot $PLOT_FILE "$@"
}


# Get data for a figure about throughput
# OUT_FILE and NB_NODES must be defined
# $1 is the PLOT_FILE that will be given to gnuplot
# The other arguments refer to the titles and the summary files
function create_thr_data {
PLOT_FILE=$1
shift

# what are the labels?
XLABEL="Checkpoint size (log scale)"
YLABEL="Throughput improvement in %"
TITLE="Checkpointing, ${NB_NODES} nodes"

complete_header $PLOT_FILE "$XLABEL" "$YLABEL" "$TITLE"

# get results
get_improvement "$@"

complete_plot $PLOT_FILE "$@"
}


function cleanup_files {
PLOT_FILE=$1
shift

for arg in "$@"; do
   title="$1"; shift
   file=$1; shift

   if [ -z "$title" ] || [ -z $file ]; then
      break
   fi

   rm ${file}.data
done

rm plot.data

rm $PLOT_FILE
}

if [ $# -ge 3 ]; then
   OUT_FILE=$1; shift
   LAT_OR_THR=$1; shift
   NB_NODES=$1; shift
else
   echo "Usage: ./$(basename $0) <out_file>.pdf <lat or thr> <given_nb_nodes> [<title> <summary_file> ...]"
   exit -1
fi

PLOT_FILE=$(mktemp gnuplot.pXXX)

if [ $LAT_OR_THR == "lat" ]; then
   create_lat_data $PLOT_FILE "$@"
elif [ $LAT_OR_THR == "thr" ]; then
   create_thr_data $PLOT_FILE "$@"
else
   echo "Unknown argument. Expecting lat or thr, got ${LAT_OR_THR}."
   rm $PLOT_FILE
   exit -1
fi

# call gnuplot
gnuplot $PLOT_FILE

cleanup_files $PLOT_FILE "$@"

# convert eps to pdf
epstopdf ${OUT_FILE}.eps
rm ${OUT_FILE}.eps

# pdfcrop on the figure
pdfcrop --margins "0 0 0 0" --clip ${OUT_FILE}.pdf ${OUT_FILE}_cropped.pdf &> /dev/null
mv ${OUT_FILE}_cropped.pdf ${OUT_FILE}.pdf

