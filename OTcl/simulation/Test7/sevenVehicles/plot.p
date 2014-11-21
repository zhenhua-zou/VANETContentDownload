
set xr [0.5:7.5]
set yr [72000:95000]
set xlabel "Vehicle Number"
set ylabel "Number of Received Packets"

plot "data.dat" using 1:2:3:4 with yerrorbars title "Individual Throughput","data.dat" using 1:5 title "Mean Throughput"

set terminal postscript eps color lw 4 "Helvetica" 15
set out 'PktsRatioTwoAPs.eps'
replot
