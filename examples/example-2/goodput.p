set title "Goodput Packet for Telnet and FTP aggregates"
set xlabel "time (s)"
set ylabel "Goodput"
set autoscale
set yrange [-0.1:1.1]
set terminal png
set out "goodput.png"
plot "Goodput.tr" using 1:2 title 'Telnet' with lines, "Goodput.tr" using 1:3 title 'FTP' with lines
