set title "Avg IPDV for EF traffic"
set xlabel "time (s)"
set ylabel "ipdv (millisec)"
set autoscale
set terminal png
set out "EF_IPDV.png"
plot "IPDV.tr" using 1:2 title 'ipdv' with points
