#ifndef __activemq_H__
#define __activemq_H__

#include <corenova/interface.h>

#include <sys/param.h>
#include <sys/socket.h>
//#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

typedef void * activemq_t;

DEFINE_INTERFACE (ActiveMQ) {

	activemq_t *	(*newAsyncConsumer) (const char brokerURI[], const char destURI[], void *fun);
	void			(*destroyAsyncConsumer) (activemq_t **);
	
	activemq_t *	(*newProducer) (const char brokerURI[], const char destURI[], const char deliveryModePersistent[]);
	void			(*destroyProducer) (activemq_t **);
	void			(*ProducerSend) (activemq_t *p, const unsigned char *msg, const int msglen);

};

#endif

