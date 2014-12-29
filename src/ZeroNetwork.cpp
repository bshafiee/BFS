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

#include "ZeroNetwork.h"
#include "LoggerInclude.h"
extern "C"{
	#include <pfring.h>
}
#include <net/if.h>   //ifreq


using namespace std;

namespace FUSESwift {

bool ZeroNetwork::initialized = false;
void* ZeroNetwork::pd = nullptr;
int ZeroNetwork::MTU;
unsigned const int ZeroNetwork::MAX_RETRY = 100000;

ZeroNetwork::ZeroNetwork() {}

ZeroNetwork::~ZeroNetwork() {}


bool ZeroNetwork::initialize(const string& _device,unsigned int _MTU) {
	initialized = false;
	MTU = _MTU;
	//Open Device
	pd = pfring_open(_device.c_str(), MTU, PF_RING_DO_NOT_PARSE);
	if(pd == NULL) {
		LOG(ERROR)<<"pfring_open "<<_device<<" error ["<<strerror(errno)<<"]";
		return false;
	} else {
		//pfring_set_application_name(pd, "BehroozFileSystem");
	}

	//Set socket mode
	pfring_set_socket_mode((pfring*)pd, send_and_recv_mode);
	pfring_set_direction((pfring*)pd, rx_and_tx_direction);//default behavior

	//wait for only 1 packet! horrible performance!
	pfring_set_poll_watermark((pfring*)pd, 0);

	//enable device
	if(pfring_enable_ring((pfring*)pd) != 0) {
	  LOG(ERROR)<<"Unable to enable ring :-(";
		pfring_close((pfring*)pd);
		return false;
	}

	u_int32_t version;
	pfring_version((pfring*)pd, &version);
	LOG(INFO)<<"Using PF_RING v."<<((version & 0xFFFF0000) >> 16)<<
	    "."<<((version & 0x0000FF00) >> 8)<<"."<<(version & 0x000000FF);

	initialized = true;
	return true;
}

void ZeroNetwork::shutDown() {
	if(pd!=nullptr)
		pfring_close((pfring*)pd);
}

int ZeroNetwork::send(char *_buffer,size_t _len) {
	unsigned int retries = 0;
redo:
	int rc = pfring_send((pfring*)pd, _buffer, _len, 1);

	if(rc == PF_RING_ERROR_INVALID_ARGUMENT) {
	  LOG(ERROR)<<"Attempting to send invalid packet[len: "<<_len<<"][MTU:"<<(((pfring*)pd)->mtu_len)<<"]";
	  return -1;
	}

	if(rc < 0) {
		retries++;
		if(retries > MAX_RETRY) {
			LOG(ERROR)<<"Send failed: "<<rc<<" MaxRetry:"<<MAX_RETRY<<" Retried:"<<retries;
			return -1;
		}
		goto redo;
	}

	return rc;
}

int ZeroNetwork::recv(u_char* _buffer,bool _block) {
	struct pfring_pkthdr hdr;
	return pfring_recv_parsed((pfring*)pd, &_buffer, MTU, &hdr, _block, 3,0,0);
}

void ZeroNetwork::getMacAndMTU(string _iface,unsigned char MAC[6],int &_MTU) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name , _iface.c_str() , IFNAMSIZ-1);
	ioctl(fd, SIOCGIFHWADDR, &ifr);
	for(unsigned int i=0;i<6;i++)
		MAC[i] = ifr.ifr_hwaddr.sa_data[i];
	ioctl(fd, SIOCGIFMTU, &ifr);
	_MTU = ifr.ifr_mtu;
	close(fd);
	printf("MTU: %d\n",ifr.ifr_mtu);
	printf("SourceMAC:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5]);
}

} /* namespace FUSESwift */
