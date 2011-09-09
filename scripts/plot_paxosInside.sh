#!/bin/bash
#
# Plot a figure with gnuplot from paxosInside mean/stdev summaries
#
# Args:
#  $1: out file
# The next arguments are optionnal and must be:
#  $i: title (e.g. communication mechanism name)
#  $i+1: summary file

if [ -z $LOG_SCALE ]; then
   LOG_SCALE=0
fi
XLABEL="Request size (log scale)"
YLABEL="Throughput in prop/sec"

# columns in the summary files for:
#  X: X-axis value
#  Y: Y-axis value
#  S: stddev value
X=3
Y=7
S=8

if [ $# -ge 1 ]; then
   OUT_FILE=$1
   shift
else
   echo "Usage: ./$(basename $0) <out_file>.pdf [<title> <summary_file> ...]"
   exit 0
fi


PLOT_FILE=$(mktemp gnuplot.pXXX)

if [ $LOG_SCALE -eq 1 ]; then
   YLABEL="${YLABEL} (log scale)"
fi

cat << EOF > $PLOT_FILE
set term postscript eps
set output "${OUT_FILE}.eps"

set xlabel "${XLABEL}"
set ylabel "${YLABEL}"
#set title "PaxosInside"

set logscale x

set xtics("1B" 1,"64B" 64,"128B" 128,"512B" 512,"1kB" 1024,"4kB" 4096,"10kB" 10240,"100kB" 102400,"1MB" 1048576)

#set key left top
#set key at 3.35,5300

set xrange [64:]

EOF

if [ $LOG_SCALE -eq 1 ]; then
   echo "set logscale y" >> $PLOT_FILE
   echo "set yrange [1:]" >> $PLOT_FILE
else
   echo "set yrange [0:]" >> $PLOT_FILE
fi


echo -n "plot " >> $PLOT_FILE

first=1
for arg in $@; do
   title="$1"; shift
   file="$1"; shift

   if [ -z "$title" ] || [ -z "$file" ]; then
      break
   fi

   if [ $first -eq 0 ]; then
      echo -n ", " >> $PLOT_FILE
   fi
   #echo -n \""$file"\" using $X:$Y:$S title \""$title"\" with yerrorlines >> $PLOT_FILE
   echo -n \""$file"\" using $X:$Y title \""$title"\" with linespoints >> $PLOT_FILE
   first=0
done

echo "" >> $PLOT_FILE

# call gnuplot
gnuplot $PLOT_FILE

rm $PLOT_FILE

# convert eps to pdf
epstopdf ${OUT_FILE}.eps
rm ${OUT_FILE}.eps

# pdfcrop on the figure
pdfcrop --margins "0 0 0 0" --clip ${OUT_FILE}.pdf ${OUT_FILE}_cropped.pdf &> /dev/null
mv ${OUT_FILE}_cropped.pdf ${OUT_FILE}.pdf
