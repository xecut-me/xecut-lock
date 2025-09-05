set -eu

# Compile
gcc *.c -o qrdemo

# Save previous version
[ -f "output.txt" ] && mv "output.txt" "output.txt.prev"

# Run current version and save output
./qrdemo > "output.txt"

# Compare if previous version exists
[ -f "output.txt.prev" ] && diff "output.txt.prev" "output.txt"

echo "Output:"
cat "output.txt"