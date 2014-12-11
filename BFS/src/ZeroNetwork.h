/*
 * ZeroNetwork.h
 *
 *  Created on: Oct 7, 2014
 *      Author: behrooz
 */

#ifndef ZERONETWORK_H_
#define ZERONETWORK_H_
#include "Global.h"
#include <string>

namespace FUSESwift {

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
	static unsigned const int MAX_RETRY = 100000;
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

} /* namespace FUSESwift */

#endif /* ZERONETWORK_H_ */
