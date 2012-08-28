#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Encapsulates various data objects as a MESSAGE, useful for passing data around.",
	.implements  = LIST ("Message","MessagePart","MessageQueue","MessageIterator","Transformation"),
	.requires    = LIST ("corenova.data.object", "corenova.data.string", "corenova.data.queue", "corenova.sys.transform"),
    .transforms  = LIST ("data:message -> data:message:queue",
                         "data:message:queue -> data:message",
                         
                         "data:message -> data:message:iterator",
                         "data:message:iterator -> data:message:part",
                         "data:message:part -> data:object::*")
};

#include <corenova/data/message.h>
#include <corenova/data/string.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h>             /* for usleep */

/*//////// Message Interface //////////////////////////////////////////*/

static message_t *
_newMessage (uint32_t id, char *from, char *to) {
	message_t *message = (message_t *)calloc (1,sizeof (message_t));
	if (message) {
		message->envelope.id   = id;
		message->envelope.from = I (String)->copy (from);
		message->envelope.to   = I (String)->copy (to);
	}
	return message;
}

static boolean_t
_attachPart (message_t *message, message_part_t *part) {
	if (message && part && message->envelope.numParts < MESSAGE_PARTS_MAXNUM) {
		message->parts = (message_part_t **)
			realloc (message->parts, sizeof (message_part_t *) * (message->envelope.numParts+1));
		if (message->parts) {
			message->parts[message->envelope.numParts] = part;
			message->envelope.numParts++;
			return TRUE;			
		}
	}
	return FALSE;
}

static message_part_t *
_getPart (message_t *message, uint16_t partNum) {
	if (message && partNum < message->envelope.numParts) {
		return message->parts[partNum];
	}
	return NULL;
}

static uint16_t
_countParts (message_t *message) {
	if (message) {
		return message->envelope.numParts;
	}
	return 0;
}

static void
_destroyMessage (message_t **messagePtr) {
	if (messagePtr) {
		message_t *message = *messagePtr;
		
		if (message) {
            DEBUGP (DDEBUG,"_destroyMessage","destroying %p",message);
            
			free (message->envelope.from);
			free (message->envelope.to);
			/* get rid of the parts */
			while (message->envelope.numParts--) {
			
				I (MessagePart)->destroy (&message->parts[message->envelope.numParts]);
			}

			free (message->parts);
			free (message);
			*messagePtr = NULL;
            
		}
	}
}

IMPLEMENT_INTERFACE (Message) = {
	.new        = _newMessage,
	.attachPart = _attachPart,
	.getPart    = _getPart,
	.countParts = _countParts,
	.destroy    = _destroyMessage
};

/*//////// MessagePart Interface //////////////////////////////////////////*/

static message_part_t *
_newMessagePart (int encoding, void *content) {
	message_part_t *part = (message_part_t *)calloc (1,sizeof (message_part_t));
	
	if (part) {
		part->encoding = encoding;
		switch (encoding) {
		case MESSAGE_BINARY: part->content.binary = (binary_t *)content; break;
		case MESSAGE_TEXT:   part->content.text   = (text_t *)content; break;
		case MESSAGE_XML:
			break;
		}
	}
	return part;
}

static void
_destroyMessagePart (message_part_t **partPtr) {
	if (partPtr) {
		message_part_t *part = *partPtr;
		if (part) {
            DEBUGP (DDEBUG,"_destroyMessage","destroying part %p",part);
			switch (part->encoding) {
			case MESSAGE_BINARY: I (BinaryObject)->destroy (&part->content.binary); break;
			case MESSAGE_TEXT:   I (TextObject)->destroy (&part->content.text); break;
			case MESSAGE_XML:
				break;
			}
			free (part);
			*partPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (MessagePart) = {
	.new     = _newMessagePart,
	.destroy = _destroyMessagePart
};

/*//////// Message Iterator Interface //////////////////////////////////////////*/

static message_iterator_t *
_newMessageIterator (message_t *message) {
    if (message) {
        message_iterator_t *iter = (message_iterator_t *) calloc (1,sizeof (message_iterator_t));
        if (iter) {
            iter->message = message;
        }
        return iter;
    }
    return NULL;
}

static message_part_t *
_nextMessageIterator (message_iterator_t *iter) {
    if (iter && iter->message) {
        if (iter->index < iter->message->envelope.numParts) {
            return I (Message)->getPart (iter->message,iter->index++);
        }
    }
    return NULL;
}

static void
_destroyMessageIterator (message_iterator_t **ptr) {
    if (ptr) {
        free (*ptr);
        *ptr = NULL;
    }
}

IMPLEMENT_INTERFACE (MessageIterator) = {
    .new     = _newMessageIterator,
    .next    = _nextMessageIterator,
    .destroy = _destroyMessageIterator
};

/*//////// Message Queue Interface //////////////////////////////////////////*/

static message_queue_t *
_newMessageQueue (uint32_t maxSize) {
	message_queue_t *queue = I (Queue)->new (maxSize/255, 255);
    I (Queue)->setBlocking (queue, TRUE);
    return (message_queue_t *)queue;
}

static message_t *
_getMessage (message_queue_t *queue) {
    return (message_t *)I (Queue)->get (queue);
}

static boolean_t 
_putMessage (message_queue_t *queue, message_t *message) {
	if (queue && message) {
        return I (Queue)->put ((cqueue_t *) queue, message);
    }
    return FALSE;

    
/* XXX - for later if we want to implement some form of LOSSY logic        
		if (I (Queue)->put (mQueue->queue, message)) {
			return TRUE;
		} else {
			if (mQueue->mode == MESSAGE_QUEUE_LOSSY) {
				message_t *old = NULL;
			  drop_again:
				old = I (Queue)->drop (mQueue->queue);
				if (old) {
					I (Message)->destroy (&old);
					if (I (Queue)->put (mQueue->queue, message)) {
						return TRUE;
					} else goto drop_again;
				} else {
					DEBUGP (DERR,"putMessage","drop failure!");
				}
			}
		}
	}
*/

}

static void
_destroyMessageQueue (message_queue_t **qPtr) {
	if (qPtr) {
		message_queue_t *queue = *qPtr;
		if (queue) {
            message_t *old;
            I (Queue)->disable (queue);
            while ((old = (message_t *) I (Queue)->drop (queue))) {
                I (Message)->destroy (&old);
            }
            I (Queue)->destroy (&queue);
			*qPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (MessageQueue) = {
	.new     = _newMessageQueue,
	.get     = _getMessage,
	.put     = _putMessage,
	.destroy = _destroyMessageQueue
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (message2iterator) {
    /*
     * conversion from data:message into data:message:iterator
     * 
     * does NOT depend on XFORM->INSTANCE
     */
    message_iterator_t *iter = I (MessageIterator)->new ((message_t *) in->data);
    if (iter) {
        transform_object_t *obj = I (TransformObject)->new ("data:message:iterator",iter);
        if (obj) {
            obj->destroy = (XDESTROY) I (MessageIterator)->destroy;
            return obj;
        }
        I (MessageIterator)->destroy (&iter);
    }
    return NULL;
}

TRANSFORM_EXEC (messageiter2part) {
    /*
     * conversion from data:message:iterator into data:message:part
     * 
     * does NOT depend on XFORM->INSTANCE
     */
    message_iterator_t *iter = (message_iterator_t *)in->data;
    if (iter) {
        message_part_t *part = I (MessageIterator)->next (iter);
        if (part) {
            DEBUGP (DDEBUG,"messageiter2part","retrieved part @ %p",part);
            transform_object_t *obj = I (TransformObject)->new ("data:message:part",part);
            if (obj) {
                I (TransformObject)->save (obj); /* don't destroy it, it's a subitem from originator */
                return obj;
            }
        }
    }
    return NULL;
}

TRANSFORM_EXEC (part2object) {
    /*
     * conversion from data:message:part into data:object::*
     *
     * RETURNS: only if xform->to matches the actual data object encoding
     * 
     * does NOT depend on XFORM->INSTANCE
     */
    DEBUGP (DDEBUG,"part2object","called with in: %p in->data: %p", in, in->data);
    
    message_part_t *part = (message_part_t *)in->data;
    if (part) {
        /* XXX - this is a hack and not a desired behavior
         * 
         * Here, we need to make a COPY of the object because data:message may prematurely get destroyed!
         */ 
        transform_object_t *obj = NULL;
        switch (part->encoding) {
          case MESSAGE_BINARY:
              obj = I (TransformObject)->new ("data:object::binary",I (BinaryObject)->clone (part->content.binary));
              break;
          case MESSAGE_TEXT:
              obj = I (TransformObject)->new ("data:object::text",I (TextObject)->clone (part->content.text));
              break;
          default:
              DEBUGP (DERR,"part2object","unsupported part->encoding: %d detected!",part->encoding);
              break;
        }
        if (obj) {
            if (I (String)->equalWild (obj->format,xform->to)) {
                switch (part->encoding) {
                  case MESSAGE_BINARY:
                      obj->destroy = (XDESTROY) I (BinaryObject)->destroy;
                      break;
                  case MESSAGE_TEXT:
                      obj->destroy = (XDESTROY) I (TextObject)->destroy;
                      break;
                }
                return obj;
            }
                      
            I (TransformObject)->destroy (&obj);
        } else {
            DEBUGP (DERR,"part2object","unable to extract object from message part!");
        }
    }
    return NULL;
}

TRANSFORM_EXEC (message2queue) {
    /*
     * conversion from data:message into data:message:queue
     *
     * places a data:message item into an instance of MessageQueue
     * this call LOOPs until the put succeeds
     *
     * RETURNS: the MessageQueue instance
     * 
     * Depend on XFORM->INSTANCE (MessageQueue)
     */
    message_queue_t *queue = (message_queue_t *)xform->instance;
    transform_object_t *obj = I (TransformObject)->new ("data:message:queue",queue);
    if (obj) {
        if (in->data != NULL) { // only put data into queue if message_t is not NULL
            while (!I (MessageQueue)->put (queue, (message_t *)in->data)) {
                usleep (500);
            }
            /*
             * SPECIAL OPERATION
             * 
             * once the message makes it into the QUEUE, we protect it
             * from being freed by previous transform
             */
            I (TransformObject)->save (in);
        }
        /*
         * the below SAVE ensures that this xform instance
         * cannot be freed.
         */
        I (TransformObject)->save (obj); /* this is an xform instance! */
        return obj;
    }
    return NULL;
}

TRANSFORM_EXEC (queue2message) {
    /*
     * conversion from data:message:queue into data:message
     *
     * gets an item from the passed in MessageQueue object
     * this call BLOCKs upto queue_timeout until the get succeeds
     * 
     * Does NOT Depend on XFORM->INSTANCE
     */
    message_queue_t *queue = (message_queue_t *)in->data;
    if (queue) {
        message_t *message = I (MessageQueue)->get (queue);
        if (message) {
            transform_object_t *obj = I (TransformObject)->new ("data:message",message);
            if (obj) {
                obj->destroy = (XDESTROY) I (Message)->destroy;
                return obj;
            }
        }
    }
    return NULL;
}

TRANSFORM_NEW (newMessageTransformation) {

    TRANSFORM ("data:message","data:message:iterator",message2iterator);
    TRANSFORM ("data:message:iterator","data:message:part",messageiter2part);
    TRANSFORM ("data:message:part","data:object::*",part2object);
    TRANSFORM ("data:message","data:message:queue",message2queue);
    TRANSFORM ("data:message:queue","data:message",queue2message);

    IF_TRANSFORM (message2queue) {
        int maxsize = MESSAGEQUEUE_DEFAULT_MAXSIZE;
        if (I (Parameters)->getValue (blueprint, "queue_maxsize")) {
            maxsize = atoi (I (Parameters)->getValue (blueprint,"queue_maxsize"));
            if (!maxsize) {
                maxsize = MESSAGEQUEUE_DEFAULT_MAXSIZE;
            }
        }
        TRANSFORM_WITH (I (MessageQueue)->new (maxsize));
    }
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyMessageTransformation) {

    IF_TRANSFORM (message2queue) {
        message_queue_t *queue = (message_queue_t *)xform->instance;
        I (MessageQueue)->destroy (&queue);
    }
        
} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newMessageTransformation,
	.destroy   = destroyMessageTransformation,
	.execute   = NULL,
	.free      = NULL
};

