#!/bin/bash

# request input
read -p "Please input a positive integer n: " n

# judge n <= 0
if [ "$n" -le 0 ]; then
	# get current time
	current_time=$(date "+%Y-%m-%d %H:%M:%S")
	# append to error.log
	echo "$current_time Input Error! n = $n" >> error.log
	# exit with code 5
	exit 5
else
	# compute sum from 1 to n
	sum=$(( n * (n + 1) / 2 ))
	# output to sum.r
	echo "The sum from 1 to $n is: $sum" > sum.r
fi
