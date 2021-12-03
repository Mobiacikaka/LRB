.PHONY : all user edge

all:
	make user
	make edge

user:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm

edge:
	g++ -g -o edge src/edge.cpp -lencrypto_utils -lboost_system -lpthread

user-random:
	g++ -g -o user_random src/user_random.cpp -lencrypto_utils -lboost_system -pthread

user-lru:
	g++ -g -o user_lru src/user_lru.cpp -lencrypto_utils -lboost_system -pthread
