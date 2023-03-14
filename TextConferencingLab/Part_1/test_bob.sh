#!/bin/bash

# Define the file name
filename="test_bob.txt"

# Check if the file exists
if [ ! -f "$filename" ]
then
    echo "Error: file does not exist"
    exit 1
fi

# Define the input values
inputs=$(cat "$filename")

# Loop through the input values and test the program
for input in "${inputs[@]}"
do
    # Print the current input value
    echo "Testing with input:"
    echo -e "$input"

    # Run the program with the current input value
    echo -e "$input" | ./client

    # Print a blank line to separate the outputs
    echo
done