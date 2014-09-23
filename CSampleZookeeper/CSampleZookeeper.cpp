//============================================================================
// Name        : CSampleZookeeper.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <zookeeper.h>
#include <proto.h>
#include <ctime>
#include <cstring>
#include <errno.h>
#include "model/ZooHandler.h"


int main(void) {

	//Wait for session establishment
	//while (sessionState!=ZOO_CONNECTED_STATE);


	/*struct Stat stat;
	struct String_vector children;
	int callResult = zoo_get_children2(zh,"/",1,&children, &stat);
	if(callResult != 0) {
		printf("zoo_get_children failed, code: %d\n",callResult);
		return -1;
	}
	//Print children
	for(int i=0;i<children.count;i++)
		printf("%s\n",children.data[i]);
	dumpStat(&stat);*/

	ZooHandler::getInstance().startElection();
	while(1);
	return EXIT_SUCCESS;
}
