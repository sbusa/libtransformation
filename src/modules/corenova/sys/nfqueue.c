#include <corenova/source-stub.h>

THIS = {
	.version     = "3.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This provides hooks into iptables NetFilter",
	.implements  = LIST ("NetfilterPacket","NetfilterQueue","Transformation"),
	.requires    = LIST ("corenova.data.parameters",
                         "corenova.data.array",
                         "corenova.data.list",
						 "corenova.data.string",
                         "corenova.data.queue",
                         "corenova.net.packet",
                         "corenova.sys.transform"),
    .transforms = LIST ("transform:feeder -> sys:nfqueue",
                        "sys:nfqueue -> sys:nfqueue:packet",
                        "sys:nfqueue:packet -> sys:nfqueue",
                        "sys:nfqueue:packet -> net:packet",
                        "net:packet -> sys:nfqueue:packet",
                        "sys:nfqueue:packet -> sys:nfqueue:filter",
                        "sys:nfqueue:filter -> sys:nfqueue:packet")
};

#include <corenova/sys/nfqueue.h>
#include <corenova/data/array.h>
#include <corenova/data/string.h>
#include <corenova/net/packet.h>
#include <corenova/sys/transform.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h>
#include <arpa/inet.h>          /* for ntohl */

#ifdef HAVE_LIBNFQUEUE

/*//////// NetfilterPacket Interface Implementation //////////////////////////////////////////*/

static nfqueue_packet_t *
newNetfilterPacket (int id, int size, char *payload, struct nfq_q_handle *handle) {
    // make a copy of the PAYLOAD!
    nfqueue_packet_t *packet = (nfqueue_packet_t *)calloc (1,sizeof (nfqueue_packet_t));
    if (packet) {
        char *buffer = malloc (size);
        if (buffer) {
            packet->data = (char *)memcpy (buffer,payload,size);
            packet->size = size;
        } else {
            DEBUGP (DERR,"newNetfilterPacket","unable to allocate copy space for packet!");
            packet->size = 0;
        }
        packet->id = id;
        packet->verdict = NF_ACCEPT; /* default verdict! */
        packet->qh = handle;
    }
    return packet;
}

static void 
setVerdictNetfilterPacket (nfqueue_packet_t *packet) {
    if (packet) {
        nfq_set_verdict (packet->qh, packet->id, packet->verdict, 0, NULL);
        packet->verdict = -1;   /* clear the verdict! */
    }
}

static void
destroyNetfilterPacket (nfqueue_packet_t **ptr) {
    if (ptr) {
        nfqueue_packet_t *packet = *ptr;
        if (packet) {
            if (packet->verdict >= 0) {
                I (NetfilterPacket)->setVerdict (packet);
            }
            free (packet->data);
            free (packet);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (NetfilterPacket) = {
    .new        = newNetfilterPacket,
    .setVerdict = setVerdictNetfilterPacket,
    .destroy    = destroyNetfilterPacket
};

/*//////// NetfilterQueue Interface Implementation //////////////////////////////////////////*/

static int nfqueue_callback (struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
                             struct nfq_data *nfa, void *data) {
    nfqueue_t *nfqueue = (nfqueue_t *)data;
    struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr (nfa);
    if (ph) {
        char *buffer;
        nfqueue_packet_t *packet = I (NetfilterPacket)->new (ntohl (ph->packet_id),
                                                             nfq_get_payload (nfa, &buffer),
                                                             buffer,
                                                             qh);
        if (packet) {
            while (!I (NetfilterQueue)->put (nfqueue,packet)) {
                usleep (500);
            }
            return 0;           /* success! */
        } else {
            DEBUGP (DERR,"nfqueue_callback","unable to initialize nfqueue_data_t structure!");
        }
    } else {
        DEBUGP (DERR,"nfqueue_callback","unable to get packet header!");
    }
    return -1;
}

static nfqueue_t *
newNetfilterQueue (parameters_t *conf) {
    if (conf) {
        if (I (Parameters)->getValue (conf,"nfqueue_num")) {
            int queue_num = atoi (I (Parameters)->getValue (conf,"nfqueue_num"));
            if (queue_num) {
                nfqueue_t *instance = (nfqueue_t *)calloc (1,sizeof (nfqueue_t));
                if (instance) {
                    int queue_size = NETFILTER_QUEUE_MAXSIZE;
                    u_int8_t queue_mode = NFQNL_COPY_PACKET;
                    char *mode = I (Parameters)->getValue (conf,"nfqueue_mode");
                    if (I (Parameters)->getValue (conf,"nfqueue_size")) {
                        queue_size = atoi (I (Parameters)->getValue (conf,"nfqueue_size"));
                        if (!queue_size || queue_size > NETFILTER_QUEUE_MAXSIZE)
                            queue_size = NETFILTER_QUEUE_MAXSIZE;
                    }
                    if (mode) {
                        if (I (String)->equal ("meta",mode)) {
                            queue_mode = NFQNL_COPY_META;
                        } else if (I (String)->equal ("full",mode)) {
                            queue_mode = NFQNL_COPY_PACKET;
                        } else if (I (String)->equal ("none",mode)) {
                            queue_mode = NFQNL_COPY_NONE;
                        } else {
                            DEBUGP (DERR,"newNetfilterQueue","invalid nfqueue_mode = %s passed in, using default 'full' mode (available: none, meta, full)",mode);
                            mode = "full";
                        }
                    } else {
                        DEBUGP (DINFO,"newNetfilterQueue","no 'nfqueue_mode' argument passed in, using default 'full' mode (available: none, meta, full)");
                        mode = "full";
                    }
                    instance->packetQueue = I (Queue)->new (queue_size/255, 255);
                    if (instance->packetQueue) {
                        struct nfq_handle *h = nfq_open ();
                        if (h) {
                            if (nfq_unbind_pf (h, AF_INET) >= 0) {
                                if (nfq_bind_pf (h, AF_INET) >= 0) {
                                    struct nfq_q_handle *qh = nfq_create_queue (h, queue_num, &nfqueue_callback, instance);
                                    if (qh) {
                                        DEBUGP (DINFO,"newNetfilterQueue","setting NetfilterQueue mode to %s",mode);
                                        if (nfq_set_mode (qh, queue_mode, 0xffff) >= 0) {
                                            I (Queue)->setBlocking (instance->packetQueue, TRUE);
                                            instance->handle = h;
                                            instance->queue = qh;
                                            instance->fd = nfnl_fd (nfq_nfnlh (h));
                                            DEBUGP (DINFO,"newNetfilterQueue","successfully created the NetfilterQueue listening on queue:%d!",queue_num);
                                            return instance;
                                        } else {
                                            DEBUGP (DERR,"newNetfilterQueue","unable to set the queue mode!");
                                        }
                                        nfq_destroy_queue (qh);
                                    } else {
                                        DEBUGP (DERR,"newNetfilterQueue","unable to create queue on %d",queue_num);
                                    }
                                } else {
                                    DEBUGP (DERR,"newNetfilterQueue","unable to bind nfnetlink_queue for AF_INET! (need to run as root?)");
                                }
                            } else {
                                DEBUGP (DERR,"newNetfilterQueue","unable to unbind existing nfnetlink_queue for AF_INET! (need to run as root?)");
                            }
                            nfq_close (h);
                        } else {
                            DEBUGP (DERR,"newNetfilterQueue","unable to open nfnetlink library handle!");
                        }
                        I (Queue)->destroy (&instance->packetQueue);
                    } else {
                        DEBUGP (DERR,"newNetfilterQueue","unable to initialize packetQueue!");
                    }
                    free (instance);
                }
            } else {
                DEBUGP (DERR,"newNetfilterQueue","invalid 'queue_num' provided as argument!");
            }
        } else {
            DEBUGP (DERR,"newNetfilterQueue","must provide 'queue_num' argument for a new NetfilterQueue!");
        }
    }
    return NULL;
}

static boolean_t
putNetfilterPacket (nfqueue_t *nfqueue, nfqueue_packet_t *packet) {
    if (nfqueue && packet) {
        return I (Queue)->put (nfqueue->packetQueue, packet);
    }
    return FALSE;
}

static nfqueue_packet_t *
getNetfilterPacket (nfqueue_t *nfqueue) {
    if (nfqueue) {
        static char buf[4096];
        if (!nfqueue->packetQueue->pending) {
            MODULE_LOCK ();
            int size = recv (nfqueue->fd, buf, sizeof (buf), 0);
            if (size >= 0) {
                nfq_handle_packet (nfqueue->handle, buf, size);
            }
            MODULE_UNLOCK ();
        }
        return (nfqueue_packet_t *)I (Queue)->get (nfqueue->packetQueue);
    }
    return NULL;
}

static void
destroyNetfilterQueue (nfqueue_t **ptr) {
    if (ptr) {
        nfqueue_t *nfqueue = *ptr;
        if (nfqueue) {
            if (nfqueue->packetQueue) {
                nfqueue_packet_t *packet;
                I (Queue)->disable (nfqueue->packetQueue);
                while ((packet = (nfqueue_packet_t *)I (Queue)->drop (nfqueue->packetQueue))) {
                    I (NetfilterPacket)->destroy (&packet);
                }
                I (Queue)->destroy (&nfqueue->packetQueue);
            }
            if (nfqueue->queue)
                nfq_destroy_queue (nfqueue->queue);
            
            if (nfqueue->handle)
                nfq_close (nfqueue->handle);

            free (nfqueue);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (NetfilterQueue) = {
    .new        = newNetfilterQueue,
    .put        = putNetfilterPacket,
    .get        = getNetfilterPacket,
    .destroy    = destroyNetfilterQueue
};

#endif  /* HAVE_LIBNFQUEUE */

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

enum xform_type {
    FEEDER2NFQUEUE = 1,
    NFQUEUE2NFPACKET,
    NFPACKET2NFQUEUE,
    NFPACKET2NETPACKET,
    NETPACKET2NFPACKET,
    NFPACKET2FILTER,
    FILTER2NFPACKET
};

static transformation_t *
newNetfilterQueueTransformation (const char *from, const char *to, parameters_t *blueprint) {
    transformation_t * xform = (transformation_t *)calloc (1, sizeof (transformation_t));
    if (xform) {
        if (from && to && blueprint) {
            /* validate from & to */
            if (I (String)->equal (from,"transform:feeder") &&
                I (String)->equal (to,  "sys:nfqueue")) {
                xform->type = FEEDER2NFQUEUE;
                xform->instance = I (NetfilterQueue)->new (blueprint);
                if (!xform->instance) {
                    DEBUGP (DERR,"newNetfilterQueueTransformation", "unable to initialize the instance for (%s -> %s)!",from, to);
                    free (xform);
                    return NULL;
                }
            }
            else if (I (String)->equal (from,"sys:nfqueue") &&
                     I (String)->equal (to,  "sys:nfqueue:packet")) {
                xform->type = NFQUEUE2NFPACKET;
            }
            else if (I (String)->equal (from,"sys:nfqueue:packet") &&
                     I (String)->equal (to,  "sys:nfqueue")) {
                xform->type = NFPACKET2NFQUEUE;
            }
            else if (I (String)->equal (from,"sys:nfqueue:packet") &&
                     I (String)->equal (to,  "net:packet")) {
                xform->type = NFPACKET2NETPACKET;
            }
            else if (I (String)->equal (from,"net:packet") &&
                     I (String)->equal (to,  "sys:nfqueue:packet")) {
                xform->type = NETPACKET2NFPACKET;
            }
            else if (I (String)->equal (from,"sys:nfqueue:packet") &&
                     I (String)->equal (to,  "sys:nfqueue:filter")) {
                xform->type = NFPACKET2FILTER;
            }
            else if (I (String)->equal (from,"sys:nfqueue:filter") &&
                     I (String)->equal (to,  "sys:nfqueue:packet")) {
                xform->type = FILTER2NFPACKET;
            }
            else {
                DEBUGP (DERR,"newNetfilterQueueTransformation", "transformation %s -> %s is not supported!", from, to);
                free (xform);
                return NULL;
            }
            
            /* proper exit */
            DEBUGP (DINFO,"newNetfilterQueueTransformation","instantiated %s -> %s transformation!", from, to);
            xform->module = SELF;
            xform->blueprint = I (Parameters)->copy (blueprint);
            xform->from = strdup (from);
            xform->to   = strdup (to);
        }
    }
    return xform;
}

static transform_object_t *
executeNetfilterQueueTransformation (transformation_t *xform, transform_object_t *in) {
    /*
     * assumes that all structures inside XFORM has been properly initialized.
     */
	if (xform) {
        switch (xform->type) {
          case FEEDER2NFQUEUE: {
              transform_object_t *obj = I (TransformObject)->new ("sys:nfqueue",xform->instance);
              if (obj) I (TransformObject)->save (obj); /* saving since copy of instance! */
              return obj;
              break;
          }
          case NFQUEUE2NFPACKET: {
              if (in && I (String)->equal (in->format,"sys:nfqueue")) {
                  nfqueue_t *nfqueue = (nfqueue_t *)in->data;
                  nfqueue_packet_t *packet = I (NetfilterQueue)->get (nfqueue);
                  if (packet) {
                      transform_object_t *obj = I (TransformObject)->new ("sys:nfqueue:packet",packet);
                      if (obj) {
                          obj->destroy = (XDESTROY) I (NetfilterPacket)->destroy;
                      }
                      return obj;
                  }
              }
              break;
          }
          case NFPACKET2NFQUEUE: { /* a consumer, never returns anything! */
              if (in && I (String)->equal (in->format,"sys:nfqueue:packet")) {
                  nfqueue_packet_t *packet = (nfqueue_packet_t *)in->data;
                  I (NetfilterPacket)->setVerdict (packet);
              }
              break;
          }
          case NFPACKET2NETPACKET: {
              if (in && I (String)->equal (in->format,"sys:nfqueue:packet")) {
                  nfqueue_packet_t *packet = (nfqueue_packet_t *)in->data;
                  net_packet_t *npkt = I (NetPacket)->new (packet->data, packet->size);
                  if (npkt) {
                      transform_object_t *obj = I (TransformObject)->new ("net:packet",npkt);
                      if (obj) {
                          obj->destroy = (XDESTROY) I (NetPacket)->destroy;
                          return obj;
                      }
                  }
              }
              break;
          }
          case NETPACKET2NFPACKET: {
              if (in && I (String)->equal (in->format,"net:packet")) {
                  //net_packet_t *npkt = (net_packet_t *)in->data;
                  transform_object_t *cur = in;
                  while ((cur = in->originator)) {
                      if (I (String)->equal (cur->format,"sys:nfqueue:packet")) {
                          transform_object_t *obj = I (TransformObject)->new ("sys:nfqueue:packet",cur->data);
                          if (obj) {
                              obj->destroy = (XDESTROY) I (NetfilterPacket)->destroy;
                              return obj;
                          }
                          break;
                      }
                  }
              }
              break;
          }
          case NFPACKET2FILTER: {
              if (in && I (String)->equal (in->format,"sys:nfqueue:packet")) {
//                  nfqueue_packet_t *packet = (nfqueue_packet_t *)in->data;
                  transform_object_t *obj = I (TransformObject)->new ("sys:nfqueue:filter",NULL);
                  if (obj) {
                      obj->destructor = SELF;
                      return obj;
                  }

                  /* 
                   * net_filter_access_t *access = I (NetFilter)->filterPacket (instance, transport);
                   * if (access) {
                   *     transform_object_t *obj = I (TransformObject)->new ("net:filter:access",access);
                   *     if (obj) {
                   *         obj->destructor = SELF;
                   *         return obj;
                   *     }
                   *     free (access);
                   * }
                   */
              }
              break;
          }
          case FILTER2NFPACKET: {
              if (in && I (String)->equal (in->format,"sys:nfqueue:filter")) {
//                  nfqueue_filter_t *filter = (nfqueue_filter_t *)in->data;
                  if (in->originator && I (String)->equal (in->originator->format,"sys:nfqueue:packet")) {
                      transform_object_t *obj = I (TransformObject)->new ("sys:nfqueue:packet",(nfqueue_packet_t *) in->originator->data);
                      if (obj) {
                          obj->destroy = (XDESTROY) I (NetfilterPacket)->destroy;
                          return obj;
                      }
                  }

              }
              break;
          }
        }
    }
	return NULL;
}

static void
destroyNetfilterQueueTransformation (transformation_t **tPtr) {
	if (tPtr) {
        transformation_t *xform = *tPtr;
        if (xform) {
            nfqueue_t *instance = (nfqueue_t *)xform->instance;
            if (instance) {
                I (NetfilterQueue)->destroy (&instance);
            }
            I (Parameters)->destroy (&xform->blueprint);
            free (xform->from);
            free (xform->to);
            free (xform);
            *tPtr = NULL;
        }
	}
}

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newNetfilterQueueTransformation,
	.destroy   = destroyNetfilterQueueTransformation,
	.execute   = executeNetfilterQueueTransformation,
	.free      = NULL
};

