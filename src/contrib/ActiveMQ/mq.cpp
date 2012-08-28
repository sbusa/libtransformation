/*
****************************************************************************************

	.version     = "1.0",
	.author      = "Keen Xiao <keen.xiao@bamboonetworks.com>",
	.description = "This module is the bridge between the C function of Corenova and
					C++ cless of ActiveMQ-CPP."
	
****************************************************************************************
*/

#include <decaf/lang/Thread.h>
#include <decaf/lang/Runnable.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/core/ActiveMQConnection.h>
#include <activemq/transport/DefaultTransportListener.h>
#include <activemq/library/ActiveMQCPP.h>
#include <decaf/lang/Integer.h>
#include <activemq/util/Config.h>
#include <decaf/util/Date.h>
#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/TextMessage.h>
#include <cms/BytesMessage.h>
#include <cms/MapMessage.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>

using namespace activemq;
using namespace activemq::core;
using namespace activemq::transport;
using namespace decaf::lang;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace cms;
using namespace std;



typedef void (*MessageHandleCallbackFuncType)(void *msg, const int msglen);


static int LibraryCount = 0;

static void InitializeLibrary(void) {

	if (LibraryCount==0) {

		activemq::library::ActiveMQCPP::initializeLibrary();

	}
	
	LibraryCount++;	
	
}


static void ShutdownLibrary() {

	if (LibraryCount>0) {

		LibraryCount--;

	}

	if (LibraryCount<=0) {
	
		activemq::library::ActiveMQCPP::shutdownLibrary();	
	}
	
}



////////////////////////////////////////////////////////////////////////////////
class SimpleAsyncConsumer : public ExceptionListener,
                            public MessageListener,
                            public DefaultTransportListener {
private:

    Connection* connection;
    Session* session;
    Destination* destination;
    MessageConsumer* consumer;
    bool useTopic;
    bool clientAck;
    std::string brokerURI;
    std::string destURI;
    
	MessageHandleCallbackFuncType MessageHandleCallbackFunc;
	
public:

	SimpleAsyncConsumer( const std::string& brokerURI,
		                 const std::string& destURI,
		                 bool useTopic = false,
		                 bool clientAck = false ) {

		this->connection = NULL;
		this->session = NULL;
		this->destination = NULL;
		this->consumer = NULL;
		this->useTopic = useTopic;
		this->brokerURI = brokerURI;
		this->destURI = destURI;
		this->clientAck = clientAck;
		
		MessageHandleCallbackFunc = NULL;
	}

	virtual ~SimpleAsyncConsumer(){
		this->cleanup();
	}
    
    void SetMessageHandle(void * fun) {
  
	    MessageHandleCallbackFunc = (MessageHandleCallbackFuncType) fun;
	    
    }

	void close() {

		this->cleanup();
	}

	int runConsumer() {

		try {

			// Create a ConnectionFactory
			ActiveMQConnectionFactory* connectionFactory = new ActiveMQConnectionFactory( brokerURI );
			

			// Create a Connection
			try {
				connection = connectionFactory->createConnection(); 
				// Keen:
				// 	blocked here for long time, if the 'brokerURI' is pointing to an unreachable host.
				// 	see http://issues.apache.org/activemq/browse/AMQCPP-168
			}
			catch (CMSException& e) {
				delete connectionFactory;
				throw e;				
			}
			
			delete connectionFactory;
			

			ActiveMQConnection* amqConnection = dynamic_cast<ActiveMQConnection*>(connection);
			if( amqConnection != NULL ) {
				amqConnection->addTransportListener( this );
			}

			connection->start();
			
			connection->setExceptionListener( this );

			// Create a Session
			if( clientAck ) {
				session = connection->createSession( Session::CLIENT_ACKNOWLEDGE );
			} else {
				session = connection->createSession( Session::AUTO_ACKNOWLEDGE );
			}

			// Create the destination (Topic or Queue)
			if( useTopic ) {
				destination = session->createTopic( destURI );
			} else {
				destination = session->createQueue( destURI );
			}

			// Create a MessageConsumer from the Session to the Topic or Queue
			consumer = session->createConsumer( destination );
			consumer->setMessageListener( this );

		} catch (CMSException& e) {

			e.printStackTrace();
			return 0;

		}
		
		return 1;
	}

    // Called from the consumer since this class is a registered MessageListener.
    virtual void onMessage( const Message* message ) {

		try {

			const BytesMessage* bytesMessage = dynamic_cast< const BytesMessage* >( message );	

			if( bytesMessage != NULL ) {
			
				std::size_t 	msglen = bytesMessage->getBodyLength(); // this line compiles and works.
				unsigned char * msgptr = bytesMessage->getBodyBytes();
				
				MessageHandleCallbackFunc(msgptr, msglen); // CallbackFunc

			} else {
			
				printf("Received BYTES MESSAGE Failed!\n");
				
			}
					
			if( clientAck ) {

				message->acknowledge();

			}
		} 
		catch (CMSException& e) {
		
			e.printStackTrace();

		}
        
    }

	// If something bad happens you see it here as this class is also been
	// registered as an ExceptionListener with the connection.
	virtual void onException( const CMSException& ex AMQCPP_UNUSED ) {
		printf("CMS Exception occurred.  Shutting down client.\n");
		exit(1);
	}

	virtual void transportInterrupted() {
		std::cout << "The Connection's Transport has been Interrupted." << std::endl;
	}

	virtual void transportResumed() {
		std::cout << "The Connection's Transport has been Restored." << std::endl;
	}

private:

    void cleanup(){

        //*************************************************
        // Always close destination, consumers and producers before
        // you destroy their sessions and connection.
        //*************************************************

        // Destroy resources.
        try{
            if( destination != NULL ) delete destination;
        }catch (CMSException& e) {}
        destination = NULL;

        try{
            if( consumer != NULL ) delete consumer;
        }catch (CMSException& e) {}
        consumer = NULL;

        // Close open resources.
        try{
            if( session != NULL ) session->close();
            if( connection != NULL ) connection->close();
        }catch (CMSException& e) {}

        // Now Destroy them
        try{
            if( session != NULL ) delete session;
        }catch (CMSException& e) {}
        session = NULL;

        try{
            if( connection != NULL ) delete connection;
        }catch (CMSException& e) {}
        connection = NULL;
    }
};


extern "C" void * ActiveMQ_AsyncConsumer_New(const char brokerURI[], const char destURI[], void *fun) {  

	InitializeLibrary();

	//============================================================
	// set to true to use topics instead of queues
	// Note in the code above that this causes createTopic or
	// createQueue to be used in the consumer.
	//============================================================
	bool useTopics = false;

	//============================================================
	// set to true if you want the consumer to use client ack mode
	// instead of the default auto ack mode.
	//============================================================
	bool clientAck = false;

	// Create the consumer
	SimpleAsyncConsumer *p = new SimpleAsyncConsumer( brokerURI, destURI, useTopics, clientAck );

	p->SetMessageHandle(fun);

	// Start it up and it will listen forever.
	if (p->runConsumer())
		return (void *)p; // Succeed
		
	// Failed
	delete p;
	return NULL;

}


extern "C" void ActiveMQ_AsyncConsumer_Destroy(void * p) { 

    SimpleAsyncConsumer *pSimpleAsyncConsumer = (SimpleAsyncConsumer *)p;
    
    pSimpleAsyncConsumer->close();
    
    delete pSimpleAsyncConsumer;

	ShutdownLibrary();

}


#include <decaf/lang/Thread.h>
#include <decaf/lang/Runnable.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/lang/Long.h>
#include <decaf/util/Date.h>

////////////////////////////////////////////////////////////////////////////////
class SimpleProducer {
private:

    Connection* connection;
    Session* session;
    Destination* destination;
    MessageProducer* producer;
    bool useTopic;
    bool clientAck;
    int deliveryMode;
    std::string brokerURI;
    std::string destURI;

public:

	enum DELIVERY_MODE {
		PERSISTENT = 0,
		NON_PERSISTENT = 1
	};


    SimpleProducer( const std::string& brokerURI,
                    const std::string& destURI,
                    int deliveryMode = NON_PERSISTENT,
                    bool useTopic = false,
                    bool clientAck = false){

        this->connection = NULL;
        this->session = NULL;
        this->destination = NULL;
        this->producer = NULL;
        this->useTopic = useTopic;
        this->brokerURI = brokerURI;        
        this->destURI = destURI;
        this->clientAck = clientAck;
        this->deliveryMode = deliveryMode;
    }

    virtual ~SimpleProducer(){
        cleanup();
    }

    void close() {
        this->cleanup();
    }
   
    //int send1(const unsigned char *msg, const int msglen) {
    int connect() {
    
		try {

			// Create a ConnectionFactory
			auto_ptr<ActiveMQConnectionFactory> connectionFactory( new ActiveMQConnectionFactory( brokerURI ) );


			// Create a Connection
			try{
				connection = connectionFactory->createConnection();
				connection->start();
			} catch( CMSException& e ) {
				e.printStackTrace();
				throw e;
			}

			// Create a Session
			if( clientAck ) {
				session = connection->createSession( Session::CLIENT_ACKNOWLEDGE );
			} else {
				session = connection->createSession( Session::AUTO_ACKNOWLEDGE );
			}

			// Create the destination (Topic or Queue)
			if( useTopic ) {
				destination = session->createTopic( destURI );
			} else {
				destination = session->createQueue( destURI );
			}

			// Create a MessageProducer from the Session to the Topic or Queue
			producer = session->createProducer( destination );
			if (deliveryMode==NON_PERSISTENT)
				producer->setDeliveryMode( DeliveryMode::NON_PERSISTENT ); // default
			else
				producer->setDeliveryMode( DeliveryMode::PERSISTENT );

			/*
				   class CMS_API DeliveryMode {
					public:

						virtual ~DeliveryMode() {}

						enum DELIVERY_MODE {
							PERSISTENT = 0,
							NON_PERSISTENT = 1
						};

					};
			 */

			/*
			{

				BytesMessage* message = session->createBytesMessage( msg, msglen );

				// Tell the producer to send the message
				producer->send( message );

				delete message;
			}
			*/

		}
		catch ( CMSException& e ) {
		
			e.printStackTrace();
			return 0;
			
		}
		
		return 1;
    }

    int send(const unsigned char *msg, const int msglen) {

		BytesMessage* message = session->createBytesMessage( msg, msglen );

		// Tell the producer to send the message
		producer->send( message );

		delete message;
    }

private:

    void cleanup(){

        // Destroy resources.
        try{
            if( destination != NULL ) delete destination;
        }catch ( CMSException& e ) { e.printStackTrace(); }
        destination = NULL;

        try{
            if( producer != NULL ) delete producer;
        }catch ( CMSException& e ) { e.printStackTrace(); }
        producer = NULL;

        // Close open resources.
        try{
            if( session != NULL ) session->close();
            if( connection != NULL ) connection->close();
        }catch ( CMSException& e ) { e.printStackTrace(); }

        try{
            if( session != NULL ) delete session;
        }catch ( CMSException& e ) { e.printStackTrace(); }
        session = NULL;

        try{
            if( connection != NULL ) delete connection;
        }catch ( CMSException& e ) { e.printStackTrace(); }
        connection = NULL;
    }
};


extern "C" void * ActiveMQ_Producer_New(const char brokerURI[], const char destURI[], const unsigned char deliveryModePersistent) {

	InitializeLibrary();

    //============================================================
    // set to true to use topics instead of queues
    // Note in the code above that this causes createTopic or
    // createQueue to be used in the consumer.
    //============================================================
    bool useTopics = false;
    
	int deliveryMode = SimpleProducer::NON_PERSISTENT;
	if (deliveryModePersistent)
		deliveryMode = SimpleProducer::PERSISTENT;

	// Create the producer and run it.
    SimpleProducer *p = new SimpleProducer( brokerURI, destURI, deliveryMode, useTopics );

    p->connect();

    return (void *)p;	
}


extern "C" void ActiveMQ_Producer_Destroy(void * p) { 

    SimpleProducer *pSimpleProducer = (SimpleProducer *)p;
    
    pSimpleProducer->close();
    
    delete pSimpleProducer;

	ShutdownLibrary();

}


extern "C" void ActiveMQ_Producer_Send(void * p, const unsigned char *msg, const int msglen) {

    SimpleProducer *pSimpleProducer = (SimpleProducer *)p;
    pSimpleProducer->send(msg, msglen);

}


