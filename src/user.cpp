#include "user.hpp"

#include <ENCRYPTO_utils/socket.h>
#include <ENCRYPTO_utils/connection.h>
#include <chrono>

lrb::User::User()
{
}

lrb::User::~User()
{
}

int lrb::User::Simulate()
{
}

int lrb::User::CachingRequest()
{
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

		// tag map lookup
		bool is_in_cache = this->LookupInMap(tmp_tag);
		// tag map admit
		if(!is_in_cache) {
			this->AdmitToCache(tmp_tag);
		}
	}
}

void lrb::User::TrainModel()
{
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
    training_loss = training_loss * 0.99 + se / kTrainingDataset * 0.01;

    LGBM_DatasetFree(dataset_handle);
    training_time = 0.95 * training_time +
                    0.05 * std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_begin).count();

}

void lrb::User::ForgetTag()
{
	auto it = memory_window.find(current_seq % kMemoryWindow);
	if(it != memory_window.end()) {
	// if(memory_window.size() >= kMemoryWindow) {
		TagType tag = it->second;	
		auto entry = tag_map.find(tag)->second;
		assert(entry.not_in_cache == kOutCacheFlag);

		auto &meta = out_cache_meta[entry.position];

		// add to training data
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

		// remove from out_cache_meta
		uint32_t tail = out_cache_meta.size() - 1;
		if(entry.position != tail) {
			out_cache_meta[entry.position] = out_cache_meta[tail];
			tag_map.find(out_cache_meta[entry.position].tag)->second.position = entry.position;
		}
		out_cache_meta.pop_back();

		tag_map.erase(tag);
		memory_window.erase(current_seq % kMemoryWindow);
	}
}

void lrb::User::Sample()
{
	size_t rand_idx = distribution(generator);
	size_t n_in = in_cache_meta.size();
	size_t n_out = out_cache_meta.size();

	std::bernoulli_distribution distribution_from_in(static_cast<double>(n_in) / (n_in + n_out));
	bool is_from_in = distribution_from_in(generator);

	if(is_from_in == kInCacheFlag) {
		uint32_t pos = rand_idx % static_cast<uint32_t>(n_in);
		auto &meta = in_cache_meta[pos];
		meta.sample_timestamps.emplace_back(current_seq);
	}
	else {
		uint32_t pos = rand_idx % static_cast<uint32_t>(n_out);
		auto &meta = out_cache_meta[pos];
		meta.sample_timestamps.emplace_back(current_seq);
	}
}

bool lrb::User::LookupInMap(TagType tag)
{
	bool ret;
	++current_seq;

	// if memory window exceeds a certain size, which means current_seq % memory_windwos_size 
	// would have already take place in memory window, so it has to be removed
	this->ForgetTag();

	auto it = tag_map.find(tag);
	if(it != tag_map.end()) { // tag has been requested recently
		unsigned int list_index = it->second.not_in_cache;
		unsigned int list_pos	= it->second.position;

		TagMeta &meta = list_index == kInCacheFlag ? in_cache_meta[list_pos] : out_cache_meta[list_pos];
		uint32_t forget_timestamp = meta.past_timestamp % kMemoryWindow;
		assert(meta.tag == tag);

		// Sample and Add to the training data
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
			memory_window.erase(forget_timestamp);
			memory_window.insert(std::make_pair<current_seq % kMemoryWindow, tag>);
		}

		ret = list_index == kInCacheFlag;

	} else { // tag has not been requested for a long time or never requested once
		ret = false;
	}

	if(this->is_sampling == true) {
		this->Sample();
	}

	return ret;
}

// * \brief Admit tag to in_cache_meta
void lrb::User::AdmitToCache(TagType tag)
{
	auto it = tag_map.find(tag);
	if(it != tag_map.end()) { // in the out cache meta
		auto &meta = out_cache_meta[it->second.position];
		uint32_t forget_timestamp = meta.past_timestamp % kMemoryWindow;
		// out_cache_meta.erase(out_cache_meta.begin() + it->second.position);
		memory_window.erase(forget_timestamp);
		auto lru_it = lru_queue.emplace_front(tag);
		in_cache_meta.emplace_back(meta, lru_it);
		it->second = {kInCacheFlag, in_cache_meta.size()-1};

		uint32_t tail = out_cache_meta.size()-1;
		if(it->second.position < tail) {
			out_cache_meta[it->second.position] = out_cache_meta[tail];
			tag_map.find(out_cache_meta[tail].tag)->second.position = it->second.position;
		}
		out_cache_meta.pop_back();
	}
	else { // new tag or removed tag
		tag_map.insert(std::make_pair(tag, MapEntry{kInCacheFlag, in_cache_meta.size()}));
		auto lru_it = lru_queue.emplace_front(tag);
		in_cache_meta.emplace_back(tag, current_seq, lru_it);
		memory_window.insert(std::make_pair(current_seq % kMemoryWindow, tag));
	}

	if(in_cache_meta.size() <= kCacheSize) {
		return ;
	}

	this->is_sampling = true;

	while(in_cache_meta.size() > kCacheSize) {
		this->EvictFromCache();
	}
}

void lrb::User::EvictFromCache()
{
	auto evict_candidate = this->RankFromCache();
	auto &meta = in_cache_meta[evict_candidate.second];

	if(kMemoryWindow <= current_seq - meta.past_timestamp) {
		// * this tag has not been call for a long time
		// * and it stays in cache, has to be removed
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

		auto entry = tag_map.find(meta.tag)->second;
		tag_map.erase(meta.tag);
		assert(entry.not_in_cache == kInCacheFlag);
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
		tag_map.find(meta.tag)->second = {kOutCacheFlag, out_cache_meta.size()-1};
		// remove from in_cache_meta
		uint32_t tail = in_cache_meta.size() - 1;
		if(evict_candidate.second < tail) {
			in_cache_meta[evict_candidate.second] = in_cache_meta[tail];
			tag_map.find(in_cache_meta[tail].tag)->second.position = evict_candidate.second;
		}
		in_cache_meta.pop_back();
	}
}

std::pair<TagType, uint32_t> lrb::User::RankFromCache()
{
	TagType tag = lru_queue.back();
	auto entry = tag_map.find(tag);
	assert(entry->second.not_in_cache == kInCacheFlag);
	auto &meta = in_cache_meta[entry->second.position];
	if(booster == nullptr || 
		current_seq - meta.past_timestamp >= kMemoryWindow) {
		return std::make_pair(tag, entry->second.position);
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
	LGBM_BoosterPredictForCSR(
		booster,
		static_cast<void *>(indptr.data()),
		C_API_DTYPE_INT32,
		indices.data(),
		static_cast<void *>(data.data()),
		C_API_DTYPE_FLOAT64,
		kSampleEviction + 1,
		data.size(),
		kMaxFeatures,
		C_API_PREDICT_NORMAL,
		0,
		0,
		prediction_params.c_str(),
		&len,
		scores
	);
	std::chrono::system_clock::time_point time_end =
		std::chrono::system_clock::now();
	prediction_time = 0.95 * prediction_time + 
		0.05 * std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_begin).count();

	auto max_score_ptr = std::max_element(scores, scores+kSampleEviction);
	auto max_score_idx = std::distance(scores, max_score_ptr);
	return std::make_pair(in_cache_meta[max_score_idx].tag, max_score_idx);
}