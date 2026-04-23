set title "Packet loss due to policy for AF1 aggregate"
set xlabel "time (s)"
set ylabel "Percentage (%)"
set autoscale
set yrange [-1:101]
set terminal png
set out "PktLoss.png"
plot "PELoss.tr" using 1:2 title 'Telnet' with lines, "PELoss.tr" using 1:3 title 'FTP' with lines
