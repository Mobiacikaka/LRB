#ifndef __EDGE_HPP__
#define __EDGE_HPP__

#include "config.hpp"

#include <unordered_set>
#include <iostream>
#include <vector>

const int kSleepTime(100); // milliseconds
const int kEdgeSize(1024);

class Edge
{
private:
	void ReadEdgeBlock(int edge_no);
	void SendEdgeBlock();

	std::unordered_set<TagType> tag_list;

public:
	Edge();
	~Edge();

	void RunEdge(int edge_no);

};

#endif
