#include "edge.hpp"

#include <ENCRYPTO_utils/socket.h>
#include <ENCRYPTO_utils/connection.h>
#include <string>
#include <unistd.h>
#include <random>
#include <fstream>
using namespace std;

Edge::Edge()
{
}

Edge::~Edge()
{
}

void Edge::RunEdge(int edge_no)
{
	this->ReadEdgeBlock(edge_no);
	this->SendEdgeBlock();
}

void Edge::ReadEdgeBlock(int edge_no)
{
	string filename = "../datasets/dataset_" + to_string(edge_no) + ".txt";
	ifstream file(filename, ios::in);

	while(!file.eof()) {
		TagType tag;
		file >> tag;
		this->tag_list.insert(tag);
	}

	file.close();
}

void Edge::SendEdgeBlock()
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
	for(auto it = tag_list.begin(); it != tag_list.end(); it ++) {
		TagType tag = *it;
		tsocket->Send((void *)&tag, sizeof(TagType));
	}

	tsocket->Close();
}

int main()
{
	clog << "Starting up Edge nodes" << endl;
	srand(time(NULL));

	int i(0);
	while(i < kEdgeNumber)
	{
		Edge newedge;
		newedge.RunEdge(i);
		usleep(rand() % kSleepTime);
		++i;
	}

	return 0;
}
