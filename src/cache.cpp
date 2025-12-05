// Sample cache implementation with random hit return
// TODO: Modify this file to model an LRU cache as in the project description

#include "cache.h"
#include <random>
#include <list>
#include <stdio.h>
#include <algorithm>

using namespace std;

// Random generator for cache hit/miss simulation
static std::mt19937 generator(42);  // Fixed seed for deterministic results
std::uniform_real_distribution<double> distribution(0.0, 1.0);

int numOffsetBits;
int numIndexBits;

// Constructor definition
Cache::Cache(CacheConfig configParam, CacheDataType cacheType) : config(configParam) {
    // Here you can initialize other cache-specific attributes
    // For instance, if you had cache tables or other structures, initialize them here
    uint64_t numSets = config.cacheSize / config.blockSize / config.ways; 
    type = cacheType;
    numOffsetBits = log2(config.blockSize); //4
    numIndexBits = log2(numSets); //6

}

// Access method definition
bool Cache::access(uint64_t address, CacheOperation readWrite) {
    uint64_t index = getIndex(address);
    cout << "address: "  << address << endl;
    cout << "index: "  << index << endl;
    uint64_t tag = getTag(address);
    auto cacheSet = cacheTable.find(index);
    cout << "getTag: "  << tag << endl;
    if (cacheSet == cacheTable.end()) {
        misses += 1;
        list<uint64_t> set = {tag};
        cacheTable.insert({index, set});
        cout << "miss: "  << address << endl;
        return false;
    }
    cout << "line 44 in cache.cpp " << endl;
    auto set = cacheSet->second;
    cout << "line 46 in cache.cpp " << endl;
    auto begin = set.begin();
    auto end = set.end();
    auto element = std::find(set.begin(), set.end(), tag);
    cout << "line 53 " << endl;
    if (element == set.end()) {
        misses += 1;
        if (set.size() < config.ways) {
            set.push_back(tag);
        } else {
            cout << "cache set is full, removing: "  << set.front() << endl;
            set.pop_front();
            set.push_back(tag);
        } 
        cacheTable[index] = set;
        cout << "miss: "  << address << endl;
        return false;
    } else {
        cout << "line line 61 in cache.cpp " << endl;
        set.remove(tag);
        cout << "line line 63 in cache.cpp " << endl;
        set.push_back(tag);
        cout << "line line 65 in cache.cpp " << endl;
        hits += 1;
        cout << "hit: "  << address << endl;
        cacheTable[index] = set;
        return true;
    }
}

// getIndex method definition
uint64_t Cache::getIndex(uint64_t address) {
    uint64_t index = address >> numOffsetBits;
    index = index << (64 - numIndexBits - numOffsetBits) >> (64 - numIndexBits - numOffsetBits);
    return index;
}

// getTag method definition
uint64_t Cache::getTag(uint64_t address) {
    uint64_t tag = address >> (numOffsetBits + numIndexBits);
    return tag;
}

// debug: dump information as you needed, here are some examples
Status Cache::dump(const std::string& base_output_name) {
    ofstream cache_out(base_output_name + "_cache_state.out");
    if (cache_out) {
        cache_out << "---------------------" << endl;
        cache_out << "Begin Register Values" << endl;
        cache_out << "---------------------" << endl;
        cache_out << "Cache Configuration:" << std::endl;
        cache_out << "Size: " << config.cacheSize << " bytes" << std::endl;
        cache_out << "Block Size: " << config.blockSize << " bytes" << std::endl;
        cache_out << "Ways: " << (config.ways) << std::endl;
        cache_out << "Miss Latency: " << config.missLatency << " cycles" << std::endl;
        cache_out << "---------------------" << endl;
        cache_out << "End Register Values" << endl;
        cache_out << "---------------------" << endl;
        return SUCCESS;
    } else {
        cerr << LOG_ERROR << "Could not create cache state dump file" << endl;
        return ERROR;
    }
}
