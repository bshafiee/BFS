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
			<<"\tAverage Block Size:"<<readAvg.avg()<<" bytes, "<<readAvg.avg()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMaximum Block Size:"<<readAvg.max()<<" bytes, "<<readAvg.max()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMinimum Block Size:"<<readAvg.min()<<" bytes, "<<readAvg.min()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tTotal read bytes:"<<readAvg.total/(1024ll*1024ll)<<" MB, "<<readAvg.total/(1024ll*1024ll*1024ll)<<" GB"<<endl;
	LOG(INFO)<<"\nSuccessful Writes Statistics:"<<endl
			<<"\tAverage Block Size:"<<writeAvg.avg()<<" bytes, "<<writeAvg.avg()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMaximum Block Size:"<<writeAvg.max()<<" bytes, "<<writeAvg.max()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tMinimum Block Size:"<<writeAvg.min()<<" bytes, "<<writeAvg.min()/(1024ll*1024ll)<<" MB"<<endl
			<<"\tTotal written bytes:"<<writeAvg.total/(1024ll*1024ll)<<" MB, "<<writeAvg.total/(1024ll*1024ll*1024ll)<<" GB"<<endl;
}
