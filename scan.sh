#!/bin/bash

# Function for Masscan scan
masscan_scan() {
    masscan -iL ip.txt -p443,80,81,82,83,84,85,86,8000,8001,8008,8080,8181,8888,9090 --exclude 255.255.255.255 -oL masscan.txt --rate=500000
    if [ $? -ne 0 ]; then
        echo "Error during masscan. Exiting."
        exit 1
    fi

    # Convert Masscan output to desired format
    output_file="ipport.txt"
    awk '/open tcp (443|80|81|82|83|84|85|86|8000|8001|8008|8080|8181|8888|9090)/ {print $4 ":" $3}' masscan.txt > "$output_file"
    if [ $? -ne 0 ]; then
        echo "Error processing masscan output. Exiting."
        exit 1
    fi
}

# Function for checker scan
httpx_scan() {
    echo "Running checker HTTP HTTPS"
    ./checker ipport.txt targets.txt
    if [ $? -ne 0 ]; then
        echo "Error during checker scan. Exiting."
        exit 1
    fi
}

# Execute the functions in the desired order
masscan_scan
httpx_scan

# Function for dir scan
./speedscan
if [ $? -ne 0 ]; then
    echo "Error during speedscan. Exiting."
    exit 1
fi

echo "All targets have been processed successfully."
echo "Mohammad Elnwajha."
