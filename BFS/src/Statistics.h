/*
 * Statistics.h
 *
 *  Created on: Dec 27, 2014
 *      Author: behrooz
 */

#ifndef SRC_STATISTICS_H_
#define SRC_STATISTICS_H_

#include <cstdint>

namespace FUSESwift {

struct StatInfo {
	int64_t total = 0;
	uint64_t counter = 0;
	int64_t maximum = 0;
	int64_t minimum = 0;
	inline uint64_t avg() {
		return (counter!=0)?total/counter:0;
	}
	inline int64_t max() {
		return maximum;
	}
	inline int64_t min() {
		return minimum;
	}
	inline void report(int64_t _elem){
		total += _elem;
		counter++;
		if(_elem > maximum)
			maximum = _elem;
		else if(_elem < minimum)
			minimum = _elem;
	}
};

class Statistics {
	static StatInfo writeAvg;
	static StatInfo readAvg;
	Statistics();
public:
	virtual ~Statistics();
	static inline void reportWrite(uint64_t _size) {
		writeAvg.report(_size);
	}
	static inline void reportRead(uint64_t _size) {
		readAvg.report(_size);
	}
	static void logStatInfo();
};

}//Namespace
#endif /* SRC_STATISTICS_H_ */
