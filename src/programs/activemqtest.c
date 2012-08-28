#include <corenova/source-stub.h>

THIS = {
    .name = "A ActiveMQ Tester",
    .version = "0.0",
    .author = "Keen Xiao <keen.xiao@bamboonetworks.com>",
    .description = "This program do some ActiveMQ testing.",
    .requires = LIST(
    					//"corenova.data.string",
	    				"corenova.net.activemq"		
    			)
};

#include <corenova/sys/getopts.h>
#include <corenova/sys/debug.h>
#include <corenova/data/stree.h>
#include <corenova/data/streams.h>
#include <corenova/data/string.h>
#include <corenova/data/object.h>
#include <corenova/data/database.h>

#include <corenova/net/activemq.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>


static void msghandler1(void *msg, const int msglen) {

	char s[255];
	strncpy(s, (char *)msg, msglen);
	s[msglen] = 0;
	
	DEBUGP (DINFO,"msghandler1", "%s[msglen=%d]\n", s, msglen);	
	
}


static void msghandler2(void *msg, const int msglen) {

	double dd;	
	memcpy(&dd, msg, msglen);
	
	DEBUGP (DINFO,"msghandler2", "%10.5f[msglen=%d]\n", dd, msglen);
	
}


int main(int argc, char **argv, char **envp) {


	DEBUGP (DDEBUG,"main", "=============Start==============");
	
	activemq_t  *pMQAsyncConsumer1 = I (ActiveMQ)->newAsyncConsumer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO1", msghandler1);
	activemq_t  *pMQAsyncConsumer2 = I (ActiveMQ)->newAsyncConsumer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO2", msghandler2);
		
	//activemq_t  *pProducer1 = I (ActiveMQ)->newProducer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO1", "false");
	//activemq_t  *pProducer2 = I (ActiveMQ)->newProducer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO2", "false");

	activemq_t  *pProducer1 = I (ActiveMQ)->newProducer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO1", "true");
	activemq_t  *pProducer2 = I (ActiveMQ)->newProducer("failover:(tcp://127.0.0.1:61616)", "TEST.FOO2", "true");

    while (1) {
    
	    const char c = getchar();
		if (c == 'q')
			break;
			
		if (c == '1'){
			char s[80];
			sprintf(s, "Hello there 1!");
			I (ActiveMQ)->ProducerSend((void *)pProducer1, (unsigned char *)s, strlen(s));
			continue;
		}
		
		if (c == '2'){
			double dd = 12.345;
			I (ActiveMQ)->ProducerSend((void *)pProducer2, (unsigned char *)(&dd), sizeof(dd));
			continue;
		}

	}
	
	DEBUGP (DDEBUG,"main", "============= Out Loop ================");
	
	if (pMQAsyncConsumer1 != NULL) {
		I (ActiveMQ)->destroyAsyncConsumer(&pMQAsyncConsumer1);
		DEBUGP (DDEBUG,"main", "AsyncConsumer 1 Destroy");
	}
	
	if (pMQAsyncConsumer2 != NULL) {
		I (ActiveMQ)->destroyAsyncConsumer(&pMQAsyncConsumer2);
		DEBUGP (DDEBUG,"main", "AsyncConsumer 2 Destroy");
	}


	if (pProducer1 != NULL) {
		I (ActiveMQ)->destroyProducer(&pProducer1);
		DEBUGP (DDEBUG,"main", "Produce 1 Destroy");
	}
	
	if (pProducer2 != NULL) {
		I (ActiveMQ)->destroyProducer(&pProducer2);
		DEBUGP (DDEBUG,"main", "Produce 2 Destroy");
	}	
	
	return 0;

}


