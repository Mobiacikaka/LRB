#ifndef __USER_HPP__
#define __USER_HPP__

#include "config.hpp"

#include <iostream>
#include <cassert>
#include <random>
#include <algorithm>
#include <cmath>

#include <vector>
#include <map>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <LightGBM/c_api.h>

namespace lrb
{

const uint32_t kMemoryWindow	= 1024 * 32;
const uint32_t kTrainingDataset	= 1024 * 64;
const uint32_t kCacheSize		= 1024 * 16;
const uint32_t kSampleEviction	= 64;

uint32_t current_seq			= -1;
const uint32_t kMaxNDelta		= 32;		// * max number of deltas
const uint32_t kMaxNEDC			= 10;		// * max number of edcs
const uint32_t kMaxFeatures		= kMaxNDelta + kMaxNEDC;

const uint8_t kInCacheFlag		= 0;
const uint8_t kOutCacheFlag		= 1;

#define CalcEDC(edc, delta, i) 	\
	(edc) = 1 + (edc) * pow(2, -(delta) / pow(2, kMaxNEDC + (i)))


typedef struct MapEntry
{
	// * 0 - tag is in cache right now
	// * 1 - tag is not in cache
	unsigned int in_cache_flag	: 1;
	unsigned int position		: 31;

	MapEntry(unsigned int in_cache_flag,
			unsigned int position) {
		this->in_cache_flag = in_cache_flag;
		this->position = position;
	}
} MapEntry;

class Feature
{
public:
	std::vector<uint32_t>	deltas;
	std::vector<float>		edcs;
	uint8_t					deltas_idx = 0;

	Feature() {
		edcs = std::vector<float>(kMaxNEDC, 0.0);
	}

	~Feature() {}

	// only when re-request this function will be called
	void Update(uint32_t delta1) {
		if(deltas.size() < kMaxNDelta) {
			deltas.push_back(delta1);
			deltas_idx = (deltas_idx++) % kMaxNDelta;
		}
		else {
			deltas[deltas_idx] = delta1;
			deltas_idx = (deltas_idx++) % kMaxNDelta;
		}
		assert(this->deltas.size() <= kMaxNDelta);

		for(size_t i = 0; i < kMaxNEDC; i ++) {
			CalcEDC(edcs[i], delta1, i);
		}
	}
};

class TagMeta
{
public:
	TagType tag;
	uint32_t past_timestamp;
	Feature features;

	std::vector<uint32_t> sample_timestamps;

	TagMeta(const TagType &tag,
			const uint32_t &past_timestamp
			) {
		this->tag = tag;
		this->past_timestamp = past_timestamp;
	}

	~TagMeta() {}

	void Update(uint32_t current_timestamp) {
		uint32_t delta1 = current_seq - this->past_timestamp;
		assert(delta1 > 0);
		features.Update(delta1);
		past_timestamp = current_timestamp;
	}

};

class InCacheTagMeta : public TagMeta {
public:
	std::list<TagType>::const_iterator lru_iterator;

	InCacheTagMeta(
		const TagMeta &meta,
		const std::list<TagType>::const_iterator lru_iterator
	): TagMeta(meta) {
		this->lru_iterator = lru_iterator;
	}

	InCacheTagMeta(
		const TagType &tag,
		const uint32_t past_timestamp,
		const std::list<TagType>::const_iterator lru_iterator
	): TagMeta(tag, past_timestamp) {
		this->lru_iterator = lru_iterator;
	}
};

class LRU_Queue {
private:
	std::list<TagType> data;

public:
	LRU_Queue() {
	}

	~LRU_Queue() {
	}

	std::list<TagType>::const_iterator emplace_front(TagType tag) {
		data.emplace_front(tag);
		return data.cbegin();
	}

	std::list<TagType>::const_iterator re_request(std::list<TagType>::const_iterator it) {
		TagType tag = *it;
		data.erase(it);
		return this->emplace_front(tag);
	}

	void erase(std::list<TagType>::const_iterator it) {
		data.erase(it);
	}

	TagType back() {
		return data.back();
	}

};

class TrainingData {
public:
	std::vector<float> labels;
	std::vector<double> data;
	std::vector<int32_t> indices;	// COL_INDEX
	std::vector<int32_t> indptr;	// ROW_INDEX

	TrainingData() {
		labels.reserve(kTrainingDataset);
		data.reserve(kTrainingDataset * kMaxFeatures);
		indices.reserve(kTrainingDataset * kMaxFeatures);
		indptr.reserve(kTrainingDataset + 1);
		indptr.emplace_back(0);
	}

	void Clear() {
		labels.clear();
		data.clear();
		indices.clear();
		indptr.resize(1);
	}

	void emplace_back(TagMeta &meta, uint32_t sample_timestamp, uint32_t future_interval) {
		int32_t indice(0);

		data.emplace_back(sample_timestamp - meta.past_timestamp);
		indices.emplace_back(indice++);

		// Insert Delta
		for(size_t i = (meta.features.deltas_idx + kMaxNDelta - 1) % kMaxNDelta;
			i < meta.features.deltas.size() && i != meta.features.deltas_idx;
			i = (i + kMaxNDelta - 1) % kMaxNDelta ) {
			// 
			data.emplace_back(meta.features.deltas[i]);
			indices.emplace_back(indice++);
		}

		assert(indice <= kMaxNDelta);
		indice = kMaxNDelta;

		// Insert EDC
		for(size_t i = 0; i < kMaxNEDC; i ++) {
			//
			auto edc = meta.features.edcs[i];
			auto delta1 = sample_timestamp - meta.past_timestamp;
			CalcEDC(edc, delta1, i);
			data.emplace_back(edc);
			indices.emplace_back(indice++);
		}

		assert(indice <= kMaxFeatures);

		indptr.emplace_back(static_cast<int32_t>(meta.features.deltas.size() - 1 + kMaxNEDC));

		// Set Label
		labels.emplace_back(log1p(future_interval));
	}
};

class User
{
private:
	size_t obj_req = 0, obj_miss = 0;
	size_t n_force_eviction = 0;
	bool is_sampling = false;

	std::default_random_engine generator = std::default_random_engine();
	std::uniform_int_distribution<size_t> distribution = std::uniform_int_distribution<size_t>();

	std::map<TagType, MapEntry> 		tag_map;
	std::vector<InCacheTagMeta>			in_cache_meta;
	std::vector<TagMeta> 				out_cache_meta;
	std::map<size_t, // current_seq % kMemoryWindow
			TagType  // tag
			> 							memory_window;
	LRU_Queue lru_queue;

	double training_loss = 0, training_time = 0, prediction_time = 0;
	uint32_t n_retrain = 0;
	TrainingData training_data;
	BoosterHandle booster = nullptr;    
	std::unordered_map<std::string, std::string> training_params_map = {
            //don't use alias here. C api may not recongize
            {"boosting",         "gbdt"},
            {"objective",        "regression"},
            {"num_iterations",   "32"},
            {"num_leaves",       "32"},
            {"num_threads",      "4"},
            {"feature_fraction", "0.8"},
            {"bagging_freq",     "5"},
            {"bagging_fraction", "0.8"},
            {"learning_rate",    "0.1"},
            {"verbosity",        "0"},
			{"force_row_wise",	 "true"},
    };
	std::string ParseMapToString() {
		std::string result;
		for(auto it = training_params_map.begin(); it != training_params_map.end(); it ++) {
			result += it->first;
			result += "=";
			result += it->second;
			result += " ";
		}
		return result;
	}
	void TrainModel();

	void Sample();
	void ForgetTag();
	bool LookupInMap(TagType tag);
	void AdmitToCache(TagType tag);
	void EvictFromCache();
	std::pair<TagType, uint32_t> RankFromCache();

	void CachingRequest();
	int GenerateUnlabledDataset();
	int GenerateLabledDataset();
	int GenerateGBMModel();

	void LogCurrentState();

public:
	User();
	~User();

	void Simulate();

};

}

#endif
