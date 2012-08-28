#ifndef __message_H__
#define __message_H__

#include <corenova/interface.h>

#include <corenova/data/object.h>

enum message_part_encoding {

    MESSAGE_BINARY,
    MESSAGE_TEXT,
    MESSAGE_XML

};

typedef struct {
	
	union {
	    binary_t *binary;
	    text_t   *text;
	} content;

	int encoding;
	
} message_part_t;

#define MESSAGE_PARTS_MAXNUM 65535

typedef struct {

	struct {

		uint32_t id;
		uint16_t numParts;
		char *from;
		char *to;
		
	} envelope;

	message_part_t **parts;
	
} message_t;

DEFINE_INTERFACE (Message) {
	message_t      *(*new)        (uint32_t id, char *from, char *to);
	boolean_t       (*attachPart) (message_t *, message_part_t *);
	message_part_t *(*getPart)    (message_t *, uint16_t partNum);
	uint16_t        (*countParts) (message_t *);
	void            (*destroy)    (message_t **);
};

DEFINE_INTERFACE (MessagePart) {
	message_part_t *(*new)     (int encoding, void *content);
	void            (*destroy) (message_part_t **);
};

/*----------------------------------------------------------------*/

typedef struct {

    message_t *message;
    unsigned char index;
    
} message_iterator_t;

DEFINE_INTERFACE (MessageIterator) {
    message_iterator_t *(*new)     (message_t *);
    message_part_t     *(*next)    (message_iterator_t *);
    void                (*destroy) (message_iterator_t **);
};

/*----------------------------------------------------------------*/

#include <corenova/data/queue.h>

typedef cqueue_t message_queue_t;

#define MESSAGEQUEUE_DEFAULT_MAXSIZE 65536

DEFINE_INTERFACE (MessageQueue) {
	message_queue_t *(*new)     (uint32_t maxSize);
	message_t       *(*get)     (message_queue_t *);
	boolean_t        (*put)     (message_queue_t *, message_t *);
	void             (*destroy) (message_queue_t **);
};

#endif	/* __message_H__ */
