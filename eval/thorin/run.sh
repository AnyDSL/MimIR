# programs=('./loopDiff' './loopDiff2')
# programs=('./loopDiff' './loopDiff2' './loopDiff_alloca' './loopDiff2_alloca')
# iterations=100
programs=('./loopDiff_large' './loopDiff2_large')
iterations=10

echo "Averaging over $iterations iterations"

for program in "${programs[@]}"; do
    if [ ! -f "$program" ]; then
        echo "File $program does not exist."
        continue
    fi
    echo "Running $program"
    sum=0
    for ((i=0; i<$iterations; i++)); do
        time=$(./$program | grep "real" | cut -f2)
        # echo "Iteration $i: $time"
        sum=$(echo "$sum + $time" | bc)
    done
    echo "Average: 0$(echo "scale=4;$sum / $iterations" | bc)s"
done
