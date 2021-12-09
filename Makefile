.PHONY : all user edge

all:
	make lrb
	make edge

edge:
	g++ -g -o edge src/edge.cpp -lencrypto_utils -lboost_system -lpthread

lrb:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_METHOD=0

lru:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_METHOD=1

random:
	g++ -g -o user src/user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm -DUSE_METHOD=2
