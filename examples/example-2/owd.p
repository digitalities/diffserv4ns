set title "Avg One-Way Delay for EF traffic"
set xlabel "time (s)"
set ylabel "one-way delay (millisec)"
set autoscale
set terminal png
set out "EF_OWD.png"
plot "OWD.tr" using 1:2 title 'One-Way delay' with points
