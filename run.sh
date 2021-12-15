#!/bin/sh

set -e

make edge

# make random
# rm -f test_random && touch test_random
# echo 'Testing Random Sample'
# for i in $(seq 1 20); do
# 	cache_size=$(($i * 256))
# 	echo cache_size: $cache_size
# 	echo $cache_size 1024 | ./user >>test_random 2>&1 & ./edge
# 	sleep 1
# done

# make lru
# rm -f test_lru && touch test_lru
# echo 'Testing LRU Sample'
# for i in $(seq 1 20); do
# 	cache_size=$(($i * 256))
# 	echo cache_size: $cache_size
# 	echo $cache_size 1024 | ./user >>test_lru 2>&1 & ./edge
# 	sleep 1
# done

# make lrb
# rm -f test_lrb && touch test_lrb
# echo 'Testing LRB Sample'
# for i in $(seq 1 20); do
# 	cache_size=$(($i * 256))
# 	train_size=$((1024 * 2))
# 	echo cache_size: $cache_size
# 	echo train_size: $train_size
# 	echo $cache_size $train_size | ./user >>test_lrb 2>&1 & ./edge
# 	sleep 1
# done

make lrb
rm -f test_lrb && touch test_lrb
echo 'Testing LRB Sample'
for i in $(seq 1 9); do
	# hf_ratio=$(echo "$((0.1 * i))")
	hf_ratio="$(echo "0.1*$i" | bc -l)"
	train_size=$((1024 * 2))
	echo "$hf_ratio" | python ./generate_datasets.py
	for j in $(seq 1 10); do
		cache_size=$((j * 128))
		echo hf_ratio: $hf_ratio, cache_size: $cache_size
		echo "$hf_ratio" >> test_lrb
		echo "$cache_size" "$train_size" | ./user >> test_lrb 2>&1 & ./edge
		sleep 1
	done
done
