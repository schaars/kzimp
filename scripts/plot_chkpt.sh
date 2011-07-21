#!/bin/bash
#
# Plot a figure with gnuplot from checkpointing mean/stdev summaries
#
# Args:
#  $1: out file
#  $2: lat or thr?
#  $3: msg_size or nb_nodes?
# The next arguments are optionnal and must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file

 
# extract data that will be plotted
function extract_data {
file=$1

# empty the output file
out=${file}.data
> $out

if [ $X_AXIS == "msg_size" ]; then
   nb_nodes=$2

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

elif [ $X_AXIS == "nb_nodes" ]; then
   msg_size=$2

   while read line; do
      grep "^[[:digit:]]\+[[:space:]]\+${msg_size}[[:space:]]" <<< $line &> /dev/null
      if [ $? -eq 0 ]; then
         nb_nodes=$(awk '{print $1}' <<< $line)

         if [ $LAT_OR_THR == "lat" ]; then
            y_value=$(awk '{print $10}' <<< $line)
            stddev=$(awk '{print $11}' <<< $line)
         elif [ $LAT_OR_THR == "thr" ]; then
            y_value=$(awk '{print $7}' <<< $line)
            stddev=$(awk '{print $8}' <<< $line)
         fi

         echo -e "${nb_nodes}\t${y_value}\t${stddev}" >> $out
      fi
   done < $file

fi
}


# Given a gnuplot script, generates the pdf
# Needs the arguments that give the title and the summary file
function generate_pdf {
PLOT_FILE=$1; shift
second_arg=$1; shift

echo -n "plot " >> $PLOT_FILE

first=1
for arg in $@; do
   title=$1; shift
   file=$1; shift

   if [ -z $title ] || [ -z $file ]; then
      break
   fi

   if [ $first -eq 0 ]; then
      echo -n ", " >> $PLOT_FILE
   fi
   
   extract_data $file $second_arg
   echo -n \"${file}.data\" using 1:2:3 title \"$title\" with yerrorlines >> $PLOT_FILE
   
   first=0
done

echo "" >> $PLOT_FILE


# call gnuplot
gnuplot $PLOT_FILE

# convert eps to pdf
epstopdf ${OUT_FILE}.eps
rm ${OUT_FILE}.eps

# pdfcrop on the figure
pdfcrop --margins "0 0 0 0" --clip ${OUT_FILE}.pdf ${OUT_FILE}_cropped.pdf &> /dev/null
mv ${OUT_FILE}_cropped.pdf ${OUT_FILE}.pdf
}

# Plot the thr/lat according to the number of nodes,
# for a given message size
# Args:
#  $1: msg size to consider
# The next arguments must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file
function plot_nb_nodes {
msg_size=$1; shift

echo msg_size=$msg_size

PLOT_FILE=$(mktemp gnuplot.pXXX)

cat << EOF > $PLOT_FILE
set term postscript eps
set output "${OUT_FILE}.eps"

set xlabel "${XLABEL}"
set ylabel "${YLABEL}"
set title "Checkpointing, msg_size = ${msg_size} bytes"

#set logscale y

#set key left top
#set key at 3.35,5300

set xrange [1:24]
set yrange [0:]
EOF

generate_pdf $PLOT_FILE $msg_size $@

for arg in $@; do
   title=$1; shift
   file=$1; shift

   if [ -z $title ] || [ -z $file ]; then
      break
   fi

   rm ${file}.data
done

rm $PLOT_FILE
mv ${OUT_FILE}.pdf ${OUT_FILE}_${LAT_OR_THR}_msgSize${msg_size}B.pdf
}


# Plot the thr/lat according to the message size,
# for a given number of nodes
# Args:
#  $1: nb of nodes to consider
# The next arguments must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file
function plot_msg_size {
nb_nodes=$1; shift

echo nb_nodes = $nb_nodes

PLOT_FILE=$(mktemp gnuplot.pXXX)

cat << EOF > $PLOT_FILE
set term postscript eps
set output "${OUT_FILE}.eps"

set xlabel "${XLABEL}"
set ylabel "${YLABEL}"
set title "Checkpointing, nb_nodes = ${nb_nodes}"

#set logscale y

set logscale x
set xtics("1B" 1,"64B" 64,"128B" 128,"512B" 512,"1kB" 1024,"4kB" 4096,"10kB" 10240,"100kB" 102400,"1MB" 1048576)

#set key left top
#set key at 3.35,5300

set xrange [128:]
set yrange [0:]
EOF

generate_pdf $PLOT_FILE $nb_nodes $@

for arg in $@; do
   title=$1; shift
   file=$1; shift

   if [ -z $title ] || [ -z $file ]; then
      break
   fi

   rm ${file}.data
done

rm $PLOT_FILE
mv ${OUT_FILE}.pdf ${OUT_FILE}_${LAT_OR_THR}_${nb_nodes}nodes.pdf
}


if [ $# -ge 3 ]; then
   OUT_FILE=$1; shift
   LAT_OR_THR=$1; shift
   X_AXIS=$1; shift
else
   echo "Usage: ./$(basename $0) <out_file>.pdf <lat or thr> <msg_size or nb_nodes> [<title> <summary_file> ...]"
   exit -1
fi


if [ $LAT_OR_THR == "lat"  ]; then
   YLABEL="Latency in usec"
elif [ $LAT_OR_THR == "thr" ]; then
   YLABEL="Throughput in snap/sec"
else
   echo -e "Unknown second argument: $2\nMust be lat or thr"
   exit -1
fi

if [ $X_AXIS == "msg_size"  ]; then
   XLABEL="Message size (log scale)"
elif [ $X_AXIS == "nb_nodes" ]; then
   XLABEL="Number of nodes"
else
   echo -e "Unknown third argument: $3\nMust be msg_size or nb_nodes"
   exit -1
fi


# construct the list of summary files
filename=0
all_filenames=""
for arg in $@; do
   if [ $filename -eq 1 ]; then
      all_filenames="${all_filenames} $arg"
   fi
   filename=$(( ($filename+1) % 2 ))
done

# NUMBER OF NODES => WE FIX THE MESSAGE SIZE
# LATENCY AND MESSAGE SIZE => WE FIX THE NUMBER OF NODES
if [ $X_AXIS == "msg_size"  ]; then
   # for all nb of nodes
   for nb_nodes in $(grep -h -v '#' $all_filenames | awk '{print $1}' | sort -n | uniq); do
      plot_msg_size $nb_nodes $@
   done

elif [ $X_AXIS == "nb_nodes" ]; then
   # for all message sizes
   for msg_size in $(grep -h -v '#' $all_filenames | awk '{print $2}' | sort -n | uniq); do
      plot_nb_nodes $msg_size $@
   done
fi

