#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Keen Xiao <keen.xiao@bamboonetworks.com>",
	.description = "This module enables ActiveMQ operations.",
	.requires    = LIST ("corenova.data.object"),
	.implements  = LIST ("ActiveMQ","Transformation"),
	.transforms  = LIST ("data:object::binary -> corenova:net:activemq")
};

#include <corenova/net/activemq.h>
#include <corenova/data/object.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/socket.h>
#include <arpa/inet.h>			/* for inet_ntoa */
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <strings.h>

/*//////// API ROUTINES //////////////////////////////////////////*/

extern void *	ActiveMQ_AsyncConsumer_New(const char brokerURI[], const char destURI[], void *fun);
extern void 	ActiveMQ_AsyncConsumer_Destroy(void * p);

static activemq_t * 
_AsyncConsumerNew(const char brokerURI[], const char destURI[], void *fun) {

	return (activemq_t *)ActiveMQ_AsyncConsumer_New(brokerURI, destURI, fun);
}


static void 
_AsyncConsumerDestroy(activemq_t **pPtr) {
    if (pPtr) {
        activemq_t *p = *pPtr;
		if (p) {
		  ActiveMQ_AsyncConsumer_Destroy((void *)p);
		  *pPtr = NULL;
		}
    }
}


//extern void *	ActiveMQ_Producer_New(const char brokerURI[], const char destURI[]);
extern void * 	ActiveMQ_Producer_New(const char brokerURI[], const char destURI[], const unsigned char deliveryModePersistent);
extern void 	ActiveMQ_Producer_Destroy(void * p);
extern void 	ActiveMQ_Producer_Send(void * p, const unsigned char *msg, const int msglen);


static activemq_t * 
_ProducerNew(const char brokerURI[], const char destURI[], const char deliveryModePersistent[]) {
  
  
	//int strcasecmp(const char *s1, const char *s2); // return zero, if strings identical
	unsigned char IsDeliveryModePersistent = 0;
	if (strcasecmp(deliveryModePersistent, "true") == 0)
		IsDeliveryModePersistent = 1;
  
	return ActiveMQ_Producer_New(brokerURI, destURI, IsDeliveryModePersistent);
}

static void
_ProducerDestroy(activemq_t **pPtr) {
  if (pPtr) {
    activemq_t *p = *pPtr;
    if (p) {
      ActiveMQ_Producer_Destroy((void *)p);
      *pPtr = NULL;	
    }
  }
}


static void 
_ProducerSend(activemq_t *p, const unsigned char *msg, const int msglen) {
  if (p) {
    ActiveMQ_Producer_Send((void *)p, msg, msglen);
  }
}


IMPLEMENT_INTERFACE (ActiveMQ) = {
	
	.newAsyncConsumer 		= _AsyncConsumerNew,
	.destroyAsyncConsumer	= _AsyncConsumerDestroy,

	.newProducer			= _ProducerNew,
	.destroyProducer		= _ProducerDestroy,
	.ProducerSend			= _ProducerSend
	
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/


TRANSFORM_EXEC (binary2mq) {
    binary_t *log = (binary_t *)in->data;
    if (log) {
		
		DEBUGP (DDEBUG,"binary2mq", "format = %s, length = %u", log->format, log->size);

		if (xform->instance) {
			activemq_t *mq = (activemq_t *)xform->instance;
			I (ActiveMQ)->ProducerSend (mq, (unsigned char *)log->data, log->size);
		}
    }
    return NULL;
}

TRANSFORM_NEW (newActiveMQTransformation) {
    
    TRANSFORM ("data:object::binary", "corenova:net:activemq", binary2mq);

    IF_TRANSFORM (binary2mq) { 
    	TRANSFORM_HAS_PARAM ("broker_host");
	TRANSFORM_HAS_PARAM ("broker_port");
    	TRANSFORM_HAS_PARAM ("queue_name");
	TRANSFORM_HAS_PARAM ("delivery_mode_persistent"); //true or false
    	
	char *server = I(Parameters)->getValue(blueprint, "broker_host");
	int port = I(Parameters)->getByteValue(blueprint, "broker_port");
	char uri[256];
	
	snprintf(uri, 256, "failover:(tcp://%s:%d)", server, port);

    	TRANSFORM_WITH (
    		I (ActiveMQ)->newProducer (uri, I(Parameters)->getValue(blueprint,"queue_name"),
			 			I(Parameters)->getValue(blueprint,"delivery_mode_persistent") )
			 			//"false" )	 // Just for test
		);
    }

} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyActiveMQTransformation) {

    IF_TRANSFORM (binary2mq) {
    	activemq_t *mq = (activemq_t *)xform->instance;
		I(ActiveMQ)->destroyProducer (&mq);
    }

} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newActiveMQTransformation,
	.destroy   = destroyActiveMQTransformation,
	.execute   = NULL,
	.free      = NULL
};

