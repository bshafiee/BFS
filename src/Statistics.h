/**********************************************************************
Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
This program comes with ABSOLUTELY NO WARRANTY;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#ifndef SRC_STATISTICS_H_
#define SRC_STATISTICS_H_

#include <cstdint>

namespace BFS {

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
