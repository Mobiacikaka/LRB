#include "config.hpp"

#include <ENCRYPTO_utils/socket.h>
#include <ENCRYPTO_utils/connection.h>
#include <iostream>
#include <cassert>
#include <random>
#include <algorithm>
#include <unordered_set>
using namespace std;

const uint32_t kCacheSize = 1024 * 16;

class User {
private:
	unordered_set<TagType> cache;
	size_t obj_req = 0, obj_miss = 0;

	bool LookupInCache(TagType tag) {
		return cache.find(tag) != cache.end();
	}

	void AdmitToCache(TagType tag) {
		cache.insert(tag);
		size_t cache_size_current = cache.size();
		if(cache_size_current >= kCacheSize) {
			size_t forget_pos = rand() % cache_size_current;
			auto it = cache.begin();
			for(size_t i = 0; i < forget_pos; i ++) it ++;
			cache.erase(it);
		}
	}

	void CachingRequest() {
		unique_ptr<CSocket> tsocket;
		tsocket = Connect(kUserIP, kUserPort);
		if(!tsocket) {
			cerr << "Connection Failed!" << endl;
			exit(1);
		}

		size_t tag_list_size(0);
		tsocket->Receive((void *)&tag_list_size, sizeof(tag_list_size));

		for(size_t i = 0; i < tag_list_size; i ++) {
			TagType tag;
			tsocket->Receive((void *)&tag, sizeof(tag));

			obj_req ++;
			if(!this->LookupInCache(tag)) {
				this->AdmitToCache(tag);
				obj_miss ++;
			}
		}
	}

public:
	User() {}
	~User() {}

	void Run() {
		for(size_t i = 0; i < kEdgeNumber; i ++) {
			cout << "Edge Node: " << i << endl;
			this->CachingRequest();
		}

		clog << "Request receive: " << obj_req << endl
			 << "Miss times: " << obj_miss << endl
			 << "Hit ratio: " << static_cast<double>(obj_req - obj_miss) / obj_req << endl;
	}
};

int main()
{
	User user;
	srand(time(NULL));
	user.Run();
	return 0;
}