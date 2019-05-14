#!/bin/bash
BENCHMARKS=(
    600.perlbench_s-210B.champsimtrace.xz
    602.gcc_s-734B.champsimtrace.xz
)
BMARKNAME=(
    perlbench
    gcc
)
PREFETCHERS=(
    no
    next_N
    #ip_stride
    #distance
    #stream
    #ampm
    #comp1
    #comp2
)
cat > out1.txt << checkpoint
{
    "Benchmark": [
checkpoint
count=0
for i in "${BENCHMARKS[@]}"
do
cat >> out1.txt << checkpoint
        {
            "Name": "${BMARKNAME[$count]}",
            "Prefetcher": [
checkpoint
for j in "${PREFETCHERS[@]}"
do
cat >> out1.txt << checkpoint
                {
checkpoint
    ./run_champsim.sh bimodal-no-$j-no-lru-1core 1 10 $i >> out1.txt
cat >> out1.txt << checkpoint
                },
checkpoint
done
cat >> out1.txt << checkpoint
            ]
        },
checkpoint
((count=count+1))
done
cat >> out1.txt << checkpoint
            ]
        }
checkpoint
