#include "edge.hpp"

#include <ENCRYPTO_utils/socket.h>
#include <ENCRYPTO_utils/connection.h>
using namespace std;

Edge::Edge()
{
}

Edge::~Edge()
{
}

int Edge::RunEdge()
{
	this->GenerateEdgeBlock();
	this->SendEdgeBlock();

	return 0;
}

int Edge::GenerateEdgeBlock()
{
	int tag_list_size(0);
	int rnd_sign(0);

	if(rand() % 2 == 0) rnd_sign = 1;
	else rnd_sign = -1;

	// taglistsize range from 9*kEdgeSize/10 to 11*kEdgeSize/10
	tag_list_size = kEdgeSize + rnd_sign * (rand() % (kEdgeSize/10));

	this->tag_list.resize(tag_list_size);

	random_device rd;
	mt19937_64 gen(rd());
	uniform_int_distribution<TagType> dist;
	for(size_t i = 0; i < tag_list_size; i ++)
	{
		tag_list[i] = dist(gen) & kTagMask;
	}

	return 0;
}

int Edge::SendEdgeBlock()
{
	unique_ptr<CSocket> tsocket;

	tsocket = Listen(kUserIP, kUserPort);
	if(!tsocket)
	{
		cerr << "Connection Failed!" << endl;
		exit(1);
	}

	size_t tag_list_size = this->tag_list.size();
	tsocket->Send((void *)&tag_list_size, sizeof(tag_list_size));
	for(size_t i = 0; i < tag_list_size; i ++)
	{
		tsocket->Send((void *)&tag_list[i], sizeof(tag_list[i]));
	}

	tsocket->Close();

	return 0;
}

int main()
{
	clog << "Starting up Edge nodes" << endl;
	srand(time(NULL));

	int i(0);
	while(i < kEdgeNumber)
	{
		Edge newedge;
		newedge.RunEdge();
		usleep(rand() % kSleepTime);
		++i;
	}

	return 0;
}
