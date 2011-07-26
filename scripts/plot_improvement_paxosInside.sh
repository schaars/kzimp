#!/bin/bash
#
# Plot a figure with gnuplot from paxosInside mean/stdev summaries,
# about the improvement of a communication mechanism over the others
#
# Args:
#  $1: out file
# The next arguments are optionnal and must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file

XLABEL="Message size (log scale)"
YLABEL="Improvement in %"

if [ $# -ge 1 ]; then
   OUT_FILE=$1
   shift
else
   echo "Usage: ./$(basename $0) <out_file>.pdf [<title> <summary_file> ...]"
   exit 0
fi


PLOT_FILE=$(mktemp gnuplot.pXXX)

# This script takes the summaries and create a new file, plot.data,
# which contains:
#  req_size    mean_improvement/A  stdev/A   mean_improvement/B   stdev/B
$(dirname $0)/plot_improvement_paxosInside.py $@

cat << EOF > $PLOT_FILE
set term postscript eps
set output "${OUT_FILE}.eps"

set xlabel "${XLABEL}"
set ylabel "${YLABEL}"
set title "PaxosInside, kZIMP improvement"

#set logscale y
set logscale x

set xtics("1B" 1,"64B" 64,"128B" 128,"512B" 512,"1kB" 1024,"4kB" 4096,"10kB" 10240,"100kB" 102400,"1MB" 1048576)

#set key left top
#set key at 3.35,5300

set xrange [64:]
#set yrange [0:]
EOF

echo -n "plot " >> $PLOT_FILE

shift; shift

first=1
Y=2
S=3
for arg in $@; do
   title=$1; shift; shift

   if [ -z $title ]; then
      break
   fi

   if [ $first -eq 0 ]; then
      echo -n ", " >> $PLOT_FILE
   fi
   #echo -n \"plot.data\" using 1:$Y:$S title \"$title\" with yerrorlines >> $PLOT_FILE
   echo -n \"plot.data\" using 1:$Y title \"$title\" with linespoint >> $PLOT_FILE
   first=0
   Y=$(($Y+2))
   S=$(($S+2))
done

echo "" >> $PLOT_FILE

# call gnuplot
gnuplot $PLOT_FILE

#rm $PLOT_FILE
rm plot.data

# convert eps to pdf
epstopdf ${OUT_FILE}.eps
rm ${OUT_FILE}.eps

# pdfcrop on the figure
pdfcrop --margins "0 0 0 0" --clip ${OUT_FILE}.pdf ${OUT_FILE}_cropped.pdf &> /dev/null
mv ${OUT_FILE}_cropped.pdf ${OUT_FILE}.pdf
