#ifndef __EDGE_HPP__
#define __EDGE_HPP__

#include "config.hpp"

#include <vector>
#include <iostream>
#include <unistd.h>
#include <random>

// const int kSleepTime(1); // seconds
const int kSleepTime(100); // milliseconds
const int kEdgeSize(1024);
// const uint64_t kTagMask(0xFFFFFFFFFFFFFFFF);
const uint64_t kTagMask(0xFFFF);

class Edge
{
private:
	int GenerateEdgeBlock();
	int SendEdgeBlock();

	std::vector<TagType> tag_list;

public:
	Edge();
	~Edge();

	int RunEdge();

};

#endif
