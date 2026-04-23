set title "Virtual queue lenght for AF1 aggregate"
set xlabel "Time (s)"
set ylabel "queue lenght (packet)"
set autoscale
set yrange [-1:40]
set terminal png
set out "VirQueueLen.png"
plot "VirQueueLen.tr" using 1:2 title 'Telnet' with points, "VirQueueLen.tr" using 1:3 title 'FTP in' with points, "VirQueueLen.tr" using 1:4 title 'FTP out' with points
