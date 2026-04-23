set title "Queue Lenght samples"
set xlabel "Time (s)"
set ylabel "queue lenght (packet)"
set autoscale
set yrange [-1:120]
set terminal png
set out "QueueLen.png"
plot "QueueLen.tr" using 1:2 title 'Premium' with points, "QueueLen.tr"using 1:3 title 'Gold' with points, "QueueLen.tr" using 1:4 title 'Best Effort' with points
