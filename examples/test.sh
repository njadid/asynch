#!/bin/sh
#$ -N SMAP_POST
#$ -j y
#$ -cwd
#$ -pe smp 4
####$ -l mf=16G
#$ -q IFC

mpirun -np 1 "python /Users/njadidoleslam/asynch (working_copy)/asynchdist.py" test.gbl
