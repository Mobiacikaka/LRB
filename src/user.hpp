#ifndef __USER_HPP__
#define __USER_HPP__

#include "config.hpp"
#include <mutex>

class User
{
private:
	// memory window
	int memory_window[kMemoryWindow];
	std::mutex mtx_memory_window;
	int MemoryWindowCaching();

	// unlabled dataset
	int unlabled_dataset[kUnlabledDataset];
	std::mutex mtx_unlabled_dataset;
	int UnlabledDatasetSampling();

	// labled dataset
	int labled_dataset[kLabledDataset];
	std::mutex mtx_labled_dataset;
	int DatasetLabling();

	// GBM

	// cache


public:
	User();
	~User();

};

#endif
