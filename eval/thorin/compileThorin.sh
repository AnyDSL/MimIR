root="../.."

echo "compile loopDiff.thorin"
$root/build/bin/thorin $root/lit/clos/loopDiff.thorin   -d clos -o loopDiff.thorin  --output-ll loopDiff.ll
echo "compile loopDiff2.thorin"
$root/build/bin/thorin $root/lit/clos/loopDiff2.thorin  -d clos -o loopDiff2.thorin --output-ll loopDiff2.ll
