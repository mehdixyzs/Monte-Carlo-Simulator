make clean
make

# Tasks 3 + 4 together: run and save CSV
./simulator -m 0 -M 15 -s 1 -e 100 -K 32 -N 128 -D "rep-hard" > sim1.csv
./simulator -m 0 -M 12 -s 1 -e 100 -K 32 -N 128 -D "rep-soft" > sim2.csv
./simulator -m 0 -M 12 -s 1 -e 100 -K 32 -N  96 -D "rep-soft" > sim3.csv
./simulator -m 0 -M 12 -s 1 -e 100 -K 32 -N  64 -D "rep-soft" > sim4.csv
./simulator -m 0 -M 12 -s 1 -e 100 -K 32 -N  32 -D "rep-soft" > sim5.csv

# Task 5
python3 plot.py
