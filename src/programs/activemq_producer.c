#include <corenova/source-stub.h>

THIS = {
    .name = "A ActiveMQ Producer",
    .version = "0.0",
    .author = "Keen Xiao <keen.xiao@bamboonetworks.com>",
    .description = "This program act as a ActiveMQ producer.",
    .requires = LIST(
						"corenova.sys.getopts",
	    				"corenova.net.activemq"		
    			),
	.options = {
		OPTION ("activemq_host", "string", "The IP address of ActiveMQ Server"),
		OPTION ("activemq_queue", "string", "The name of ActiveMQ Queue you want to send msg to"),
        OPTION ("msg_str", "string", "The msg you want to send"),
		OPTION_NULL
	}

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


/* command line options */
static char *str1 = NULL;
static char *str2 = NULL;
static char *str3 = NULL;

/*
./activemq_producer --activemq_host="failover:(tcp://127.0.0.1:61616)" --activemq_queue="cpn.test" --msg_str="Hello World"
*/

int main(int argc, char **argv) {

	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);	

	if (params && params->count==3) {
		str1 = I (Parameters)->getValue (params,"activemq_host");
		str2 = I (Parameters)->getValue (params,"activemq_queue");
		str3 = I (Parameters)->getValue (params,"msg_str");
		
		printf("\tactivemq_host:%s\n\tactivemq_queue:%s\n\tmsg_str:%s\n", str1, str2, str3);		
	} else {
		DEBUGP (DFATAL,"main","run with -h for help in running this program.");
		return 0;
	}

	activemq_t  *pProducer = I (ActiveMQ)->newProducer(str1, str2, "false");
	
	if (pProducer==NULL) {
		DEBUGP (DFATAL,"main","Failed to new a Producer.");
		return 0;
	}

	I (ActiveMQ)->ProducerSend((void *)pProducer, (unsigned char *)str3, strlen(str3));

	if (pProducer != NULL) {
		I (ActiveMQ)->destroyProducer(&pProducer);
		DEBUGP (DDEBUG,"main", "Produce Destroy");		
	}	

	return 1;
}

