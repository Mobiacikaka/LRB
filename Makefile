.PHONY : all user edge

all:
	make user
	make edge

edge:
	g++ -g -o edge src/edge.cpp -lencrypto_utils -lboost_system -lpthread

user:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_MODE=0

lru:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_MODE=1

random:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_MODE=2
