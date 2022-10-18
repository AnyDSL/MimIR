programs=('./loopDiff' './loopDiff2')
iterations=100

for program in "${programs[@]}"; do
    echo "Running $program"
    sum=0
    for ((i=0; i<$iterations; i++)); do
        time=$(./$program | grep "real" | cut -f2)
        # echo "Iteration $i: $time"
        sum=$(echo "$sum + $time" | bc)
    done
    echo "Average: $(echo "scale=4;$sum / $iterations" | bc)"
done
