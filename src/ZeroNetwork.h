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

#ifndef ZERONETWORK_H_
#define ZERONETWORK_H_
#include "Global.h"
#include <string>

namespace BFS {

class ZeroNetwork {
protected:
	ZeroNetwork();
	/** private static member **/
	static bool initialized;
	static void *pd;
public:
	virtual ~ZeroNetwork();
	/** public static member **/
	static int MTU;
	static const unsigned int MAX_RETRY;
	/** public static Functions **/
	static bool initialize(const std::string &_device,unsigned int _MTU);
	static void shutDown();
	/**
	 * Sends data over the interface!
	 * Important your data should not be longer that 'SEND_LENGTH'
	 * if it's bigger it'll be ignored and -1 returned.
	 * it's user responsibility to shrink data to 'SEND_LENGTH'
	 *
	 * @return number of sent bytes or a negative number in case
	 * of error.
	 */
	static int send(char *_buffer,size_t _len);

	/**
	 * _buffer should have at least 'SEND_LENGTH' bytes
	 * memory allocated!
	 *
	 * @return number of bytes received, or a negative number
	 * in case of error.
	 */
	static int recv(u_char* _buffer,bool _block=true);

	static void getMacAndMTU(std::string _iface,unsigned char MAC[6],int &_MTU);
};

} /* namespace BFS */

#endif /* ZERONETWORK_H_ */
