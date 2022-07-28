#include "user.hpp"

#include <ENCRYPTO_utils/socket.h>
#include <ENCRYPTO_utils/connection.h>
#include <fstream>
#include <chrono>
#include <exception>

lrb::User::User(const size_t cache_size, const size_t training_data_size):
	kCacheSize(cache_size),
	kTrainingDataset(training_data_size)
{
	training_data = TrainingData(kTrainingDataset);
}

lrb::User::~User()
{
}

void lrb::User::LogCurrentState()
{
	std::cerr << "current sequence: " << current_seq << std::endl;                    
	std::cerr << "current cache size: " << in_cache_meta.size() << std::endl;         
	std::cerr << "current memory window size: " << memory_window.size() << std::endl; 
	std::cerr << "tag map size: " << tag_map.size() << std::endl;                     

	std::ofstream file;
	
	file.open("in_cache_meta.log", std::ios::out);
	for(size_t i = 0; i < in_cache_meta.size(); i ++) {
		file << in_cache_meta[i].tag << "\t" << in_cache_meta[i].past_timestamp << std::endl;
	}
	file.close();

	file.open("out_cache_meta.log", std::ios::out);
	for(size_t i = 0; i < out_cache_meta.size(); i ++) {
		file << out_cache_meta[i].tag << "\t" << out_cache_meta[i].past_timestamp << std::endl;
	}
	file.close();

	file.open("memory_window.log", std::ios::out);
	for(auto it = memory_window.begin(); it != memory_window.end(); it ++) {
		file << it->first << "\t" << it->second << std::endl;
	}
	file.close();

	file.open("tag_map.log", std::ios::out);
	for(auto it = tag_map.begin(); it != tag_map.end(); it ++) {
		file << it->first << "\t" << it->second.in_cache_flag << "\t" << it->second.position << std::endl;
	}
}

#define DEBUG

#ifdef DEBUG
#define LOG_POSITION() \
	std::clog << "[LRB] [LOG] Function " << __ASSERT_FUNCTION << " in Line " << __LINE__ << std::endl;

#define FUNC_BEGIN() \
	try { \
		// LOG_POSITION();

#define FUNC_END()                                                                    \
	}                                                                                 \
	catch (std::exception & e)                                                        \
	{                                                                                 \
		std::cerr << "Exception caught: " << e.what() << std::endl;                   \
		std::cerr << "And error occured. Exiting " << __ASSERT_FUNCTION << std::endl; \
		throw e;																	  \
	} \
	// LOG_POSITION();

#else
#define LOG_POSITION()
#define FUNC_BEGIN()
#define FUNC_END()
#endif

void lrb::User::Simulate()
{
FUNC_BEGIN();

#ifdef USE_METHOD
	std::clog << "User using mode: " << USE_METHOD << std::endl;
#else
	std::cerr << "USE_METHOD Undefined! System Exiting!" << std::endl;
	return;
#endif

	for(size_t i = 0; i < kEdgeNumber; i ++) {
		// std::cout << "Edge Node: " << i << std::endl;
		this->CachingRequest();
	}
	std::clog 
		<< "Request receive: " << obj_req << std::endl
		<< "Miss times: " << obj_miss << std::endl
		<< "Hit ratio: " << static_cast<double>(obj_req - obj_miss) / obj_req << std::endl;
FUNC_END();
}

void lrb::User::CachingRequest()
{
FUNC_BEGIN();
	std::unique_ptr<CSocket> tsocket;
	tsocket = Connect(kUserIP, kUserPort);
	if(!tsocket) {
		std::cerr << "Connection Failed!" << std::endl;
		exit(1);
	}

	size_t tag_list_size(0);
	tsocket->Receive((void*)&tag_list_size, sizeof(tag_list_size));

	// ! Simulation Main Body
	for(size_t i = 0; i < tag_list_size; i ++) {
		TagType tmp_tag;
		tsocket->Receive((void *)&tmp_tag, sizeof(tmp_tag));

		obj_req ++;
		// tag map lookup
		bool in_cache_flag = this->LookupInMap(tmp_tag);
		// tag map admit
		if(in_cache_flag == false) {
			this->AdmitToCache(tmp_tag);
			obj_miss ++;
		}
	}

	tsocket->Close();
FUNC_END();
}

void lrb::User::TrainModel()
{
FUNC_BEGIN();
	std::clog << "[LRB] [Warning] Model Start Training" << std::endl;

	++n_retrain;
	auto time_begin = std::chrono::system_clock::now();
	std::string training_params = ParseMapToString();

	if(booster) LGBM_BoosterFree(booster);
	booster = nullptr;

	DatasetHandle dataset_handle;
	LGBM_DatasetCreateFromCSR(
		static_cast<void *>(training_data.indptr.data()),
		C_API_DTYPE_INT32,
		training_data.indices.data(),
		static_cast<void *>(training_data.data.data()),
		C_API_DTYPE_FLOAT64,
		training_data.indptr.size(),
		training_data.data.size(),
		kMaxFeatures,
		training_params.c_str(),
		nullptr,
		&dataset_handle
	);

	LGBM_DatasetSetField(
		dataset_handle,
		"label",
		static_cast<void *>(training_data.labels.data()),
		training_data.labels.size(),
		C_API_DTYPE_FLOAT32
	);

	LGBM_BoosterCreate(
		dataset_handle, 
		training_params.c_str(), 
		&booster
	);
	// train dataset
	for(int i = 0; i < std::stoi(training_params_map["num_iterations"]); i ++) {
		int is_finished;
		LGBM_BoosterUpdateOneIter(booster, &is_finished);
		if(is_finished) 
			break;
	}

	int64_t len;
	std::vector<double> result(training_data.indptr.size() - 1);
	LGBM_BoosterPredictForCSR(
		booster,
		static_cast<void *>(training_data.indptr.data()),
		C_API_DTYPE_INT32,
		training_data.indices.data(),
		static_cast<void *>(training_data.data.data()),
		C_API_DTYPE_FLOAT64,
		training_data.indptr.size(),
		training_data.data.size(),
		kMaxFeatures,
		C_API_PREDICT_NORMAL,
		0, // ! start index
		0,
		training_params.c_str(),
		&len,
		result.data()
	);

    double se = 0;
    for (int i = 0; i < result.size(); ++i) {
        auto diff = result[i] - training_data.labels[i];
        se += diff * diff;
    }
    /* training_loss = training_loss * 0.99 + se / kTrainingDataset * 0.01; */

    LGBM_DatasetFree(dataset_handle);
    /* training_time = 0.95 * training_time + */
    /*                 0.05 * std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_begin).count(); */

FUNC_END();
}

void lrb::User::ForgetTag()
{
FUNC_BEGIN();
	auto it = memory_window.find(current_seq % kMemoryWindow);
	if(it != memory_window.end()) {
		TagType tag = it->second;	
		auto entry = tag_map.find(tag)->second;
		auto &meta = out_cache_meta[entry.position];
		assert(meta.tag == tag && entry.in_cache_flag == kOutCacheFlag);

		// add to training data
#if USE_METHOD == 0
		if(meta.sample_timestamps.empty() == false) {
			uint32_t future_distance = kMemoryWindow * 2;
			for(auto sample_timestamp : meta.sample_timestamps) {
				training_data.emplace_back(meta, sample_timestamp, future_distance);
			}
			if(training_data.labels.size() >= kTrainingDataset) {
				this->TrainModel();
				this->training_data.Clear();
			}
			meta.sample_timestamps.clear();
			meta.sample_timestamps.shrink_to_fit();
		}
#endif

		// remove from out_cache_meta
		if(entry.position < out_cache_meta.size() - 1) {
			meta = out_cache_meta.back();
			tag_map.find(meta.tag)->second.position = entry.position;
		}
		out_cache_meta.pop_back();

		tag_map.erase(tag);
		memory_window.erase(current_seq % kMemoryWindow);
	}
FUNC_END();
}

void lrb::User::Sample()
{
FUNC_BEGIN();
	size_t rand_idx = distribution(generator);
	size_t n_in = in_cache_meta.size();
	size_t n_out = out_cache_meta.size();

	std::bernoulli_distribution distribution_from_in(static_cast<double>(n_in) / (n_in + n_out));
	bool is_from_in = distribution_from_in(generator);

	if(is_from_in == kInCacheFlag || !n_out) {
		uint32_t pos = rand_idx % static_cast<uint32_t>(n_in);
		auto &meta = in_cache_meta[pos];
		meta.sample_timestamps.emplace_back(current_seq);
	}
	else {
		uint32_t pos = rand_idx % static_cast<uint32_t>(n_out);
		auto &meta = out_cache_meta[pos];
		meta.sample_timestamps.emplace_back(current_seq);
	}
FUNC_END();
}

bool lrb::User::LookupInMap(TagType tag)
{
FUNC_BEGIN();
	bool ret;
	++current_seq;

	// if memory window exceeds a certain size, which means current_seq % memory_windwos_size 
	// would have already take place in memory window, so it has to be removed
	this->ForgetTag();

	auto it = tag_map.find(tag);
	if(it != tag_map.end()) { // tag has been requested recently
		unsigned int list_index = it->second.in_cache_flag;
		unsigned int list_pos	= it->second.position;

		TagMeta &meta = list_index == kInCacheFlag ? in_cache_meta[list_pos] : out_cache_meta[list_pos];
		uint32_t forget_timestamp = meta.past_timestamp % kMemoryWindow;
		assert(meta.tag == tag);

		// Sample and Add to the training data
#if USE_METHOD == 0
		if(meta.sample_timestamps.empty() == false) {
			for(auto sample_timestamp : meta.sample_timestamps) {
				uint32_t future_distance = current_seq - sample_timestamp;
				training_data.emplace_back(meta, sample_timestamp, future_distance);
			}
			if(training_data.labels.size() >= kTrainingDataset) {
				this->TrainModel();
				training_data.Clear();
			}
			meta.sample_timestamps.clear();
			meta.sample_timestamps.shrink_to_fit();
		}
#endif

		meta.Update(current_seq);

		if(list_index == kInCacheFlag) {
			// tag is in the cache
			// Update LRU queue
			auto &meta = in_cache_meta[list_pos];
			meta.lru_iterator = lru_queue.re_request(meta.lru_iterator);
		}
		else {
			// tag is not in the cache
			// but it is in the memory window
			// Update memory window
			assert(memory_window.find(forget_timestamp) != memory_window.end());
			memory_window.erase(forget_timestamp);
			memory_window.insert(std::make_pair(current_seq % kMemoryWindow, tag));
			assert(memory_window.find(current_seq % kMemoryWindow) != memory_window.end());
		}

		ret = list_index == kInCacheFlag;

	} else { // tag has not been requested for a long time or never requested once
		ret = false;
	}

	if(this->is_sampling == true) {
#if USE_METHOD == 0
		this->Sample();
#endif
	}

	return ret;
FUNC_END();
}

// * \brief Admit tag to in_cache_meta
void lrb::User::AdmitToCache(TagType tag)
{
FUNC_BEGIN();
	auto it = tag_map.find(tag);
	if(it != tag_map.end()) {
		// in the out cache meta
		// bring meta from out_cache_meta to in_cache_meta
		auto &meta = out_cache_meta[it->second.position];
		assert(meta.tag == tag);
		uint32_t forget_timestamp = meta.past_timestamp % kMemoryWindow;
		auto lru_it = lru_queue.emplace_front(tag);
		in_cache_meta.emplace_back(meta, lru_it);

		memory_window.erase(forget_timestamp);
		if(it->second.position < out_cache_meta.size() - 1) {
			meta = out_cache_meta.back();
			tag_map.find(meta.tag)->second.position = it->second.position;
		}
		out_cache_meta.pop_back();

		it->second = {kInCacheFlag, static_cast<uint32_t>(in_cache_meta.size())};
	}
	else { // new tag or removed tag
		tag_map.insert(
			std::make_pair(
				tag, MapEntry{
					kInCacheFlag,
					static_cast<uint32_t>(in_cache_meta.size())
				}
			)
		);
		auto lru_it = lru_queue.emplace_front(tag);
		in_cache_meta.emplace_back(tag, current_seq, lru_it);
	}

	if(in_cache_meta.size() <= kCacheSize) {
		return ;
	}

	this->is_sampling = true;

	while(in_cache_meta.size() > kCacheSize) {
		this->EvictFromCache();
	}
FUNC_END();
}

void lrb::User::EvictFromCache()
{
FUNC_BEGIN();
	auto evict_candidate = this->RankFromCache(); // {tag, position in in_cache_meta}
	auto &meta = in_cache_meta[evict_candidate.second];

	if(kMemoryWindow <= current_seq - meta.past_timestamp) {
		// * this tag has not been call for a long time
		// * and it stays in cache, has to be removed
#if USE_METHOD == 0
		if(meta.sample_timestamps.empty() == false) {
			uint32_t future_distance = current_seq - meta.past_timestamp + kMemoryWindow;
			for(auto sample_timestamp : meta.sample_timestamps) {
				training_data.emplace_back(meta, sample_timestamp, future_distance);
			}
			if(training_data.labels.size() >= kTrainingDataset) {
				this->TrainModel();
				training_data.Clear();
			}
			meta.sample_timestamps.clear();
			meta.sample_timestamps.shrink_to_fit();
		}
#endif

		auto entry = tag_map.find(meta.tag)->second;
		assert(entry.in_cache_flag == kInCacheFlag);

		tag_map.erase(meta.tag);
		lru_queue.erase(meta.lru_iterator);

		uint32_t tail = in_cache_meta.size() - 1;
		if(entry.position < tail) {
			in_cache_meta[entry.position] = in_cache_meta[tail];
			tag_map.find(in_cache_meta[tail].tag)->second.position = entry.position;
		}
		in_cache_meta.pop_back();
		n_force_eviction ++;
	}
	else {
		// Add to memory window
		memory_window.insert(std::make_pair(meta.past_timestamp % kMemoryWindow, meta.tag));
		// Add to out_cache_meta
		out_cache_meta.emplace_back(meta);
		// Remove from LRU Queue
		lru_queue.erase(meta.lru_iterator);
		// change Entry in tag_map
		tag_map.find(meta.tag)->second = MapEntry{kOutCacheFlag, static_cast<uint32_t>(out_cache_meta.size()-1)};
		auto &entry = tag_map.find(meta.tag)->second;
		entry.in_cache_flag = kOutCacheFlag;
		entry.position = out_cache_meta.size() - 1;
		// remove from in_cache_meta
		uint32_t tail = in_cache_meta.size() - 1;
		if(evict_candidate.second < tail) {
			in_cache_meta[evict_candidate.second] = in_cache_meta[tail];
			tag_map.find(in_cache_meta[tail].tag)->second.position = evict_candidate.second;
		}
		in_cache_meta.pop_back();
	}
FUNC_END();
}

std::pair<TagType, uint32_t> lrb::User::RankFromCache()
{
FUNC_BEGIN();
	TagType tag = lru_queue.back();
	auto entry = tag_map.find(tag);
#if USE_METHOD == 1
	return std::make_pair(tag, static_cast<uint32_t>(entry->second.position));
#elif USE_METHOD == 2
	uint32_t position = static_cast<uint32_t>(rand()) % in_cache_meta.size();
	return std::make_pair(in_cache_meta[position].tag, position);
#endif
	assert(entry->second.in_cache_flag == kInCacheFlag);
	auto &meta = in_cache_meta[entry->second.position];
	if(booster == nullptr || 
		current_seq - meta.past_timestamp >= kMemoryWindow) {
		return std::make_pair(tag, static_cast<uint32_t>(entry->second.position));
	}

	// random sample
	std::unordered_set<uint32_t> pos_set;
	size_t cache_size = in_cache_meta.size();
	while(pos_set.size() < kSampleEviction) {
		uint32_t pos = distribution(generator) % cache_size;
		pos_set.insert(pos);
	}

	// Create Predict Dataset
	std::vector<double> data;
	std::vector<int32_t> indices;
	std::vector<int32_t> indptr;
	for(auto it = pos_set.begin(); it != pos_set.end(); it ++) {
		// 
		int32_t indice(0);
		auto &meta = in_cache_meta[*it];

		data.emplace_back(current_seq - meta.past_timestamp);
		indices.emplace_back(indice++);

		for(size_t i = (meta.features.deltas_idx + kMaxNDelta - 1) % kMaxNDelta;
			i < meta.features.deltas.size() && i != meta.features.deltas_idx;
			i = (i + kMaxNDelta - 1) % kMaxNDelta) {
			data.emplace_back(meta.features.deltas[i]);
			indices.emplace_back(indice++);
		}

		assert(indice <= kMaxNDelta);
		indice = kMaxNDelta;

		for(size_t i = 0; i < kMaxNEDC; i ++) {
			auto edc = meta.features.edcs[i];
			auto delta1 = current_seq - meta.past_timestamp;
			CalcEDC(edc, delta1, i);
			data.emplace_back(edc);
			indices.emplace_back(indice++);
		}

		assert(indice <= kMaxFeatures);

		indptr.emplace_back(static_cast<int32_t>(meta.features.deltas.size() - 1 + kMaxNEDC));
	}

	int64_t len;
	double scores[kSampleEviction];
	std::chrono::system_clock::time_point time_begin =
		std::chrono::system_clock::now();
	std::string prediction_params = this->ParseMapToString();
	// LOG_POSITION();
	LGBM_BoosterPredictForCSR(
		booster,
		static_cast<void *>(indptr.data()),
		C_API_DTYPE_INT32,
		indices.data(),
		static_cast<void *>(data.data()),
		C_API_DTYPE_FLOAT64,
		indptr.size(),
		data.size(),
		kMaxFeatures,
		C_API_PREDICT_NORMAL,
		0,
		0,
		prediction_params.c_str(),
		&len,
		scores
	);
	// LOG_POSITION();
	std::chrono::system_clock::time_point time_end =
		std::chrono::system_clock::now();
	prediction_time = 0.95 * prediction_time + 
		0.05 * std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_begin).count();

	auto max_score_ptr = std::max_element(scores, scores+kSampleEviction);
	auto max_score_idx = std::distance(scores, max_score_ptr);
	return std::make_pair(in_cache_meta[max_score_idx].tag, max_score_idx);
FUNC_END();
}

int main()
{
	size_t cache_size(0);
	std::cin >> cache_size;

	size_t training_data_size(0);
	std::cin >> training_data_size;

	std::clog << "cache size: " << cache_size << std::endl
			<< "training data size: " << training_data_size << std::endl;

	lrb::User user(cache_size, training_data_size);
	std::srand(time(NULL));
	user.Simulate();

	std::clog << std::endl;
	return 0;
}
