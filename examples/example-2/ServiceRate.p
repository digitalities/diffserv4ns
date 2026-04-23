set title "Service rate"
set xlabel "time (s)"
set ylabel "bandwidth (kbps)"
set autoscale
set terminal png
set out "ServiceRate.png"
plot "ServiceRate.tr" using 1:2 title 'Premium' with lines, "ServiceRate.tr" using 1:3 title 'Gold' with lines,"ServiceRate.tr" using 1:4 title 'Best Effort' with lines

