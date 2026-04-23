set title "Class rate"
set xlabel "time (s)"
set ylabel "bandwidth (kbps)"
set autoscale
set terminal png
set out "ClassRate.png"
plot "ClassRate.tr" using 1:2 title 'EF aggregate' with lines, "ClassRate.tr" using 1:3 title 'Telnet aggregate' with lines, "ClassRate.tr" using 1:4 title 'FTP aggregate' with lines, "ClassRate.tr" using 1:5 title 'Best Effort aggregate' with lines

