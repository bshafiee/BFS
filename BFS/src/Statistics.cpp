/*
 * Statistics.cpp
 *
 *  Created on: Dec 27, 2014
 *      Author: behrooz
 */

#include "Statistics.h"
#include "LoggerInclude.h"

using namespace std;
using namespace FUSESwift;

StatInfo Statistics::readAvg;
StatInfo Statistics::writeAvg;

Statistics::Statistics() {}

Statistics::~Statistics() {}

void Statistics::logStatInfo() {
	LOG(INFO)<<"\nSuccessful Reads Statistics:"<<endl
			<<"\tAverage:"<<readAvg.avg()<<" bytes, "<<readAvg.avg()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMaximum:"<<readAvg.max()<<" bytes, "<<readAvg.max()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMinimum:"<<readAvg.min()<<" bytes, "<<readAvg.min()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tTotal:"<<readAvg.total/(1024ll*1024ll)<<" MB, "<<readAvg.total/(1024ll*1024ll*1024ll)<<" GB"<<endl;
	LOG(INFO)<<"\nSuccessful Writes Statistics:"<<endl
			<<"\tAverage:"<<writeAvg.avg()<<" bytes, "<<writeAvg.avg()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMaximum:"<<writeAvg.max()<<" bytes, "<<writeAvg.max()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMinimum:"<<writeAvg.min()<<" bytes, "<<writeAvg.min()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tTotal:"<<writeAvg.total/(1024ll*1024ll)<<" MB, "<<writeAvg.total/(1024ll*1024ll*1024ll)<<" GB"<<endl;
}
