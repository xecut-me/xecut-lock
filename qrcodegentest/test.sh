set -eu

# Compile
gcc *.c -O2 -o qrdemo

# Log compile results
echo "Demo successfully compiled. Output of ls -hal:"
ls -hal ./qrdemo
echo

# Save previous version
[ -f "output.txt" ] && mv "output.txt" "output.txt.prev"

# Run current version and save output
./qrdemo > "output.txt"

# Compare if previous version exists
[ -f "output.txt.prev" ] && diff "output.txt.prev" "output.txt"

echo "Output of current version:"
cat "output.txt"