#!/bin/bash


# Check if the correct number of arguments is provided
if [ $# -ne 2 ]; then
    echo "Error: Missing parameters. Usage: $0 <writefile> <writestr>"
    exit 1
fi

writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Try to create or overwrite the file with the provided text
echo "$writestr" > "$writefile"

# Check if the file was successfully created
if [ $? -ne 0 ]; then
    echo "Error: Could not create file $writefile."
    exit 1
fi

echo "Successfully wrote to $writefile."
exit 0

