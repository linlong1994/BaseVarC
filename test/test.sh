#!/bin/bash

time ../src/BaseVarC basetype --rerun -q 20 -t 4 -b 10 -i bam.list -s chr17:41197700-41276155 -r data/chr17.fa.gz -o test.out >test.sh.o 2>test.sh.e
