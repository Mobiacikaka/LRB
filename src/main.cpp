#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

/*
volatile int i(0);
mutex mtx;

void t1()
{
	while(i <= 50)
	{
		if(mtx.try_lock())
		{
			cout << "t1: " << i << endl;
			++i;
			mtx.unlock();
		}
	}
}

void t2()
{
	while(i <= 50)
	{
		if(mtx.try_lock())
		{
			cout << "t2: " << i << endl;
			++i;
			mtx.unlock();
		}
	}
}

int main()
{
	thread T1(t1);
	thread T2(t2);
	T1.join();
	T2.join();

	return 0;
}
*/

class multithread
{
private:
	volatile int i;
	mutex mtx_i;
	void t1();
	void t2();

public:
	multithread();
	~multithread();
	void run();
};

multithread::multithread()
{
	i = 0;
}

multithread::~multithread()
{
	
}

void multithread::run()
{
	thread T1(&multithread::t1, this);
	thread T2(&multithread::t2, this);
	T1.join();
	T2.join();
}

void multithread::t1()
{
	while(i < 50)
	{
		if(mtx_i.try_lock()) {
			cout << "t1: " << i << endl;
			++i;
			mtx_i.unlock();
		}
	}
}

void multithread::t2()
{
	while(i < 50)
	{
		if(mtx_i.try_lock()) {
			cout << "t2: " << i << endl;
			++i;
			mtx_i.unlock();
		}
	}
}

int main()
{
	multithread mt;
	mt.run();
	return 0;
}
