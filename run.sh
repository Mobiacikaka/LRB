#!/bin/sh

set -e

make edge
# python ./generate_datasets.py

# make random
# log_file="test_random"
# rm -f $log_file && touch $log_file
# echo 'Testing Random Sample'
# for i in $(seq 1 20); do
# 	cache_size=$(($i * 128))
# 	echo cache_size: $cache_size
# 	echo $cache_size 1024 | ./user >> $log_file 2>&1 & ./edge
# 	sleep 1
# done

# make lru
# log_file="test_lru"
# rm -f $log_file && touch $log_file
# echo 'Testing LRU Sample'
# for i in $(seq 1 20); do
# 	cache_size=$(($i * 128))
# 	echo cache_size: $cache_size
# 	echo $cache_size 1024 | ./user >> $log_file 2>&1 & ./edge
# 	sleep 1
# done

# make lrb
# log_file="test_lrb"
# # rm -f $log_file && touch $log_file
# echo 'Testing LRB Sample'
# for i in $(seq 5 10); do
# 	cache_size=$(($i * 128))
# 	train_size=$((1024 * 16))
# 	echo cache_size: $cache_size
# 	echo train_size: $train_size
# 	echo $cache_size $train_size | ./user >> $log_file 2>&1 & ./edge
# 	sleep 1
# done

# make lrb
# log_file="test_lrb"
# rm -f $log_file && touch $log_file
# echo 'Testing LRB Sample'
# for i in $(seq 1 9); do
# 	hf_ratio="$(echo "0.1*$i" | bc -l)"
# 	train_size=$((1024 * 2))
# 	echo "$hf_ratio" | python ./generate_datasets.py
# 	for j in $(seq 1 10); do
# 		cache_size=$((j * 128))
# 		echo hf_ratio: $hf_ratio, cache_size: $cache_size
# 		echo "$hf_ratio" >> $log_file
# 		echo "$cache_size" "$train_size" | ./user >> $log_file 2>&1 & ./edge
# 		sleep 1
# 	done
# done

# fig 2.2
make lrb
log_file="test_lrb"
echo 'Testing LRB Sample'
for i in 1 2 4 8; do
	cache_size=$((i * 128))
	for j in 1 2 4 8 16 32 64; do
		train_size=$((j * 128))
		echo cache_size: $cache_size, train_size: $train_size
		echo "$cache_size" "$train_size" | ./user >> $log_file 2>&1 & ./edge
		sleep 1
	done
done

