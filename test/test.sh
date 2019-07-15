#!/bin/bash

#time ../src/BaseVarC basetype --rerun --keep_tmp -g group.info.txt -t 4 -b 10 -i bam.list -s chr17:41197700-41276155 -r data/chr17.fa.gz -o test.out >test.sh.o 2>test.sh.e
time ../src/BaseVarC basetype -t 8 -b 10 -g group.info.txt -i bam.list -s chr17:41197700-41276155 -r data/chr17.fa.gz -o test.out >test.sh.o 2>test.sh.e
