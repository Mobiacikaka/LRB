.PHONY : all user edge

all:
	make user
	make edge

user:
	g++ -g -o user user.cpp -lencrypto_utils -lboost_system -lpthread -l _lightgbm

edge:
	g++ -g -o edge edge.cpp -lencrypto_utils -lboost_system -lpthread

