#include <corenova/source-stub.h>

THIS = {
    .name = "A ActiveMQ Asyn Consumer",
    .version = "0.0",
    .author = "Keen Xiao <keen.xiao@bamboonetworks.com>",
    .description = "This program do some ActiveMQ testing.",
    .requires = LIST(
						"corenova.sys.getopts",    					
	    				"corenova.net.activemq"
    			),
    			
	.options = {
		OPTION ("activemq_host", "string", "The IP address of ActiveMQ Server"),
		OPTION ("activemq_queue", "string", "The name of ActiveMQ Queue you want to get msg from"),
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


static void msghandler(void *msg, const int msglen) {

	char s[255];
	strncpy(s, (char *)msg, msglen);
	s[msglen] = 0;
	
	DEBUGP (DINFO,"msghandler", "Reveive Msg:%s\n", s);
	DEBUGP (DINFO,"msghandler", "Msg Length :%-d\n", msglen);		
	
}


/* command line options */
static char *str1 = NULL;
static char *str2 = NULL;

/*
./activemq_consumer --activemq_host="failover:(tcp://127.0.0.1:61616)" --activemq_queue="cpn.test"
*/


int main(int argc, char **argv, char **envp) {


	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);	

	if (params && params->count==2) {
	
		str1 = I (Parameters)->getValue (params,"activemq_host");
		str2 = I (Parameters)->getValue (params,"activemq_queue");
		printf("\tactivemq_host:%s\n\tactivemq_queue:%s\n", str1, str2);

	} else {
	
		DEBUGP (DFATAL,"main","run with -h for help in running this program.");
		return 0;
		
	}
			
	activemq_t  *pMQAsyncConsumer = I (ActiveMQ)->newAsyncConsumer(str1, str2, msghandler);

		
	DEBUGP (DINFO,"main", "Press 'q' to Exit.");

    while (1) {
    
	    const char c = getchar();
		if (c == 'q')
			break;

	}
	
	DEBUGP (DDEBUG,"main", "Jump Out Loop");
	
	if (pMQAsyncConsumer != NULL) {
		I (ActiveMQ)->destroyAsyncConsumer(&pMQAsyncConsumer);
		DEBUGP (DDEBUG,"main", "AsyncConsumer Destroy");
	}
	
	return 0;

}


