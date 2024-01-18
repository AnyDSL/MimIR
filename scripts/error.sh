# call `./error.sh [-o] -n [output lines] [command]`

if [ "$1" = "-o" ]; then
    out_only=true
    shift
else
    out_only=false
fi

if [ "$1" = "-n" ]; then
    lines=$2
    shift 2
else
    lines=20
fi

command="$@"
current_dir=$(pwd)

# Run the command, store the output and the exit code
if [ "$out_only" = true ]; then
    output=$($command 2> /dev/null)
else
    output=$($command 2>&1)
fi
exit_code=$?

# get the last 5 lines of the output
output=$(echo "$output" | tail -n $lines)

# replace current directory with $PWD
output=${output//$current_dir/\$PWD}

# get current branch and repository url
remote=$(git config --get remote.origin.url)
branch=$(git rev-parse --abbrev-ref HEAD)
commit=$(git rev-parse HEAD)

echo "<details>"
echo "<summary>Information</summary>"
echo ""
echo "remote: $remote"
echo "branch: $branch"
echo "commit: $commit"
echo ""
echo "Call: \`$command\`"
echo ""
echo "\`\`\`rust"
echo "$output"
echo "\`\`\`"
echo "</details>"
