
set style data linespoints

set xlabel "Time (s)"
set ylabel "Objective function J(t)"
plot "f.txt" u 2:8  w l t ''

