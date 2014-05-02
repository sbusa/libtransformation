#include <corenova/source-stub.h>

THIS = {
	.version     = "1.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "This is a module that handles a data pipe between file descriptors",
    .requires    = LIST ("corenova.data.array","corenova.net.socket","corenova.sys.loader","corenova.sys.transform", "corenova.net.ssl"),
	.implements  = LIST ("DataPipe","DataPipeStream","StreamIntercept","Transformation"),
    .transforms  = LIST ("data:pipe:stream -> data:pipe:stream@flush",
                         "data:pipe:stream -> data:pipe:intercept",
                         "data:pipe:intercept -> data:pipe:stream",
                         "data:pipe:stream -> data:pipe::request*",
                         "data:pipe:stream -> data:pipe::response*",
                         "data:pipe -> data:pipe@flush",
                         "data:pipe::* -> data:pipe@flush")
};

#include <corenova/data/pipe.h>
#include <corenova/data/array.h>
#include <corenova/sys/transform.h>

/*//////// DataPipe Interface Implementation //////////////////////////////////////////*/

#include <unistd.h>              /* for read & write */
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

#define STREAM_INTERCEPT_MAX 8192

static boolean_t _intercepts[STREAM_INTERCEPT_MAX];
static boolean_t _hasInitialized = FALSE;

/*//////// DataPipe Interface Implementation //////////////////////////////////////////*/

static data_pipe_t *
newDataPipe (int in, int out) {
    if (in && out) {
        data_pipe_t *pipe = (data_pipe_t *)calloc (1,sizeof (data_pipe_t));
        if (pipe) {
            pipe->fds[0].fd = in;
            pipe->fds[1].fd = out;
            pipe->buf = (char *) malloc (sizeof (char) * PIPE_BLOCK_SIZE + 1);
            if (!pipe->buf) {
                free (pipe);
                return NULL;
            }
            pipe->bufend = pipe->buf + sizeof (char) *PIPE_BLOCK_SIZE;
            pipe->pos = pipe->dataend = pipe->buf;

            /* setup some defaults */
            pipe->maxsize = PIPE_BUF_MAXSIZE;

            DEBUGP (DDEBUG,"newDataPipe","(%d -> %d) @ %p",in,out,pipe);
        }
        return pipe;
    }
    return NULL;
}

static int
replaceInput (data_pipe_t *pipe, int newIn) {
    if (pipe && newIn) {
        int oldIn = pipe->fds[0].fd;
        pipe->fds[0].fd = newIn;
        return oldIn;
    }
    return -1;
}

static int
replaceOutput (data_pipe_t *pipe, int newOut) {
    if (pipe && newOut) {
        int oldOut = pipe->fds[1].fd;
        pipe->fds[1].fd = newOut;
        return oldOut;
    }
    return -1;
}

static void
reverseDataPipe (data_pipe_t *pipe) {
    if (pipe) {
        int oldOut = pipe->fds[1].fd;
        pipe->fds[1].fd = pipe->fds[0].fd;
        pipe->fds[0].fd = oldOut;

        if (pipe->ssl[0] && pipe->ssl[1]) {
            ssl_t *temp = pipe->ssl[0];
            pipe->ssl[1] = pipe->ssl[0];
            pipe->ssl[0] = temp;		
	}	
    }
}

static void
setMaxSize (data_pipe_t *pipe, uint32_t size) {
    if (pipe && size) {
        pipe->maxsize = size;
    }
}

static void
setMarker (data_pipe_t *pipe, char *pos) {
    if (pipe && pos && pos >= pipe->pos && pos <= pipe->dataend) {
        pipe->marker = pos;
    }
}

static int
pollDataPipe (data_pipe_t *pipe, pipe_poll_type_t type, int timeout) {
    if (pipe) {
        struct timeval tv1, tv2;
        
        /* reset to PIPE_POLLINOUT (reverse logic...)*/
        pipe->fds[0].events = POLLIN;
        pipe->fds[1].events = POLLOUT;
        
        switch (type) {
          case PIPE_POLLIN:    pipe->fds[1].events = 0; break;
          case PIPE_POLLOUT:   pipe->fds[0].events = 0; break;
          case PIPE_POLLINOUT: break;
        }
        
        while (TRUE) {
            gettimeofday (&tv1,NULL);
            switch (poll (pipe->fds, 2, timeout)) {
              case -1:
                  if (errno == EINTR && !SystemExit) {
                      if (timeout > 0) {
                          gettimeofday (&tv2,NULL);
                          timeout -= ((tv2.tv_sec - tv1.tv_sec) * 1000) + ((tv2.tv_usec - tv1.tv_usec) / 1000);
                          if (timeout > 0) continue;
                      } else continue;   
                  }
              case 0:
                  return PIPE_TIMEOUT;
            }
            //DEBUGP (DDEBUG,"pollDataPipe","poll result for %d:%d and %d:%d",pipe->fds[0].fd,pipe->fds[0].revents,pipe->fds[1].fd,pipe->fds[1].revents);
            /*
             * first, handle error cases related to INPUT & OUTPUT pipes
             */
            if (pipe->fds[1].revents & POLLERR || pipe->fds[1].revents & POLLHUP || pipe->fds[1].revents & POLLNVAL) {
                DEBUGP (DWARN,"pollDataPipe","output pipe has issue! (this is fatal)");
                /*
                 * something's wrong with the output pipe!
                 *
                 * otherwise, if we've written everything we have, then no problem.
                 * otherwise, receiving endpoint should not prematurely close connection!
                 */
                return PIPE_FATAL;
                //if (pipe->pos == pipe->dataend) return PIPE_BROKEN;
                //else return PIPE_FATAL;
            }
    
            if (pipe->fds[0].revents & POLLERR || pipe->fds[0].revents & POLLHUP || pipe->fds[0].revents & POLLNVAL) {
                if (!(pipe->fds[0].revents & POLLIN)) {
                    DEBUGP (DWARN,"pollDataPipe","input pipe has issue (nothing to read?)!");
                } else {
                    DEBUGP (DWARN,"pollDataPipe","input pipe has issue (has something to read)!");
                }
                /*
                 * something's wrong with the input pipe!
                 *
                 * we're going to assume end of pipe, but first check if there's stuff to read or write.
                 */
                return PIPE_BROKEN;

                /*
                if (pipe->pos == pipe->dataend) return PIPE_BROKEN;
                else return PIPE_FATAL;
        
                if (!(pipe->fds[0].events & POLLIN)) {
                    if (pipe->pos == pipe->dataend) return PIPE_BROKEN;
                    //state = PIPE_WRITEDATA; // explicitly jump to PIPE_WRITEDATA state! (since there WILL NOT be any more input)
                }
                */
            }
            return PIPE_CONTINUE;
        }
    }
    return PIPE_FATAL;
}

static boolean_t
fillDataPipe (data_pipe_t *pipe, char *buf, uint32_t size) {
    if (pipe && buf && size) {
      pipe_fill_buffer:
        if (size > (pipe->bufend - pipe->dataend)) {
            int bufsize = pipe->bufend - pipe->buf;
            /* reached max of currently allocated buffer */
            if (bufsize < pipe->maxsize) { // have some space to grow!
                int pos_offset = pipe->pos - pipe->buf;
                DEBUGP (DDEBUG,"fillDataPipe","growing buffer");
                pipe->buf = (char *) realloc (pipe->buf, bufsize + sizeof (char) * PIPE_BLOCK_SIZE + 1);
                pipe->bufend = pipe->buf + bufsize + sizeof(char) * PIPE_BLOCK_SIZE;
                pipe->dataend = pipe->buf + bufsize;
                pipe->pos = pipe->buf + pos_offset;
            } else {
                DEBUGP (DERR,"fillDataPipe","overflow! only %d available!", bufsize);
                return FALSE;
            }
            goto pipe_fill_buffer;
        }
        memcpy (pipe->dataend, buf, size);
        pipe->dataend += size;
        return TRUE;
    }
    return FALSE;
}

static boolean_t
hasDataInPipe (data_pipe_t *pipe) {
    if (pipe && (pipe->dataend - pipe->pos > 0)) {
        return TRUE;
    }
    return FALSE;
}

static void
resetDataPipe (data_pipe_t *pipe) {
    if (pipe) {
        DEBUGP (DDEBUG,"resetDataPipe","clearing pipe buffers!");
        free (pipe->buf);
        pipe->bufend = pipe->dataend = pipe->pos = pipe->buf = NULL;
    }
}

static int
readDataPipe (data_pipe_t *pipe) {
    if (pipe) {
        int nread = 0;
        if (pipe->pos == pipe->dataend) { /* no more data to write */
            pipe->pos = pipe->dataend = pipe->buf; /* soft reset */
        }
                      
        if (pipe->dataend == pipe->bufend) {
            int bufsize = pipe->bufend - pipe->buf;
            /* reached max of currently allocated buffer */
            if (bufsize < pipe->maxsize) { // have some space to grow!
                int pos_offset = pipe->pos - pipe->buf;
                DEBUGP (DDEBUG,"readDataPipe","%p - growing buffer to %u",pipe,bufsize + (unsigned int) sizeof (char) * PIPE_BLOCK_SIZE);
                pipe->buf = (char *) realloc (pipe->buf, bufsize + sizeof (char) * PIPE_BLOCK_SIZE + 1);
                pipe->bufend = pipe->buf + bufsize + sizeof(char) * PIPE_BLOCK_SIZE;
                pipe->dataend = pipe->buf + bufsize;
                pipe->pos = pipe->buf + pos_offset;
            }
        }
                  
        if (pipe->bufend - pipe->dataend > 0) {
            /* read some data */
            if (pipe->ssl[0]) {
               // DEBUGP (DDEBUG, "readDataPipe", "read ssl data");
               if ((nread = I(SSLConnector)->read(pipe->ssl[0], &pipe->dataend, pipe->bufend- pipe->dataend)) < 0) {
                   DEBUGP (DERR,"readDataPipe","%p - unable to read data from input pipe!",pipe);
                return PIPE_FATAL;
               }

            } else if ((nread = read (pipe->fds[0].fd, pipe->dataend, pipe->bufend - pipe->dataend)) < 0) {
                DEBUGP (DERR,"readDataPipe","%p - unable to read data from input pipe!",pipe);
                return PIPE_FATAL;
            }
            if (nread == 0) {
                if (pipe->pos == pipe->dataend) {
                    DEBUGP (DDEBUG,"readDataPipe","%p - input terminated, nothing to write, we're done!",pipe);
                    return PIPE_BROKEN;
                } else {
                    DEBUGP (DDEBUG,"readDataPipe","%p - nothing to read, but we have %lu  bytes to write!",pipe, (pipe->dataend - pipe->pos));
                    return PIPE_NEEDWRITE;
                }
            }
            pipe->dataend += nread;
            *pipe->dataend = '\0';
            DEBUGP (DDEBUG,"readDataPipe","%p - read %d bytes",pipe,nread);
        } else {
            /* there's more data to read, but we're out of space! */
            if (pipe->pos == pipe->dataend) { // no more data to write
                DEBUGP (DDEBUG,"readDataPipe","shrinking buffer");
                // shrink buffer so we can start fresh!
                pipe->buf = (char *) realloc (pipe->buf, sizeof (char) * PIPE_BLOCK_SIZE + 1);
                pipe->bufend = pipe->buf + sizeof (char) * PIPE_BLOCK_SIZE;
                pipe->pos = pipe->dataend = pipe->buf; // reset pos and dataend to beginning of buf
            } else { // we have more data to write, what's going on?
                DEBUGP (DDEBUG,"readDataPipe","more to read, but buffer is full!");
                return PIPE_NEEDWRITE;
            }
        }
        return PIPE_CONTINUE;
    }
    return PIPE_FATAL;
}

static int
writeDataPipe (data_pipe_t *pipe) {
    if (pipe) {
        int nwrite = 0;
        if (pipe->dataend - pipe->pos > 0) {
            if (pipe->ssl[1]) {
            /* write some data */
               // DEBUGP (DDEBUG, "writeDataPipe", "Ravi: sending data on SSL in mode %d", pipe->ssl[1]->mode);
               if ((nwrite = I(SSLConnector)->write(pipe->ssl[1], pipe->pos, pipe->dataend - pipe->pos)) < 0) {
                   DEBUGP (DERR,"writeDataPipe","%p - unable to write data to output pipe!",pipe);
                // fatal failure during sending data!
                return PIPE_FATAL;
               }
            } else {
                if ((nwrite = write (pipe->fds[1].fd, pipe->pos, pipe->dataend - pipe->pos)) < 0) {
                    DEBUGP (DERR,"writeDataPipe","%p - unable to write data to output pipe!",pipe);
                    // fatal failure during sending data!
                    return PIPE_FATAL;
                }
            }
#ifdef DEBUG_INTENSIVE
            /* debugging */
            int i = 0;
            while (i < nwrite) {
                fprintf (stderr,"%c", *(pipe->pos + i));
                i++;
            }
#endif                          
            pipe->pos += nwrite;
            DEBUGP (DDEBUG,"writeDataPipe","%p - wrote %d bytes",pipe,nwrite);
        } else {
            DEBUGP (DDEBUG,"writeDataPipe","%p - sent all data from input -> output!",pipe);
            return PIPE_COMPLETE;
        }
        return PIPE_CONTINUE;
    }
    return PIPE_FATAL;
}

/*
 * a special call that will write a defined region from pipe
 */
static int
writeRegionDataPipe (data_pipe_t *pipe, char *spos, char *epos) {
    if (pipe && spos && epos && epos > spos) {
        int nwrite = 0;
        while (epos - spos > 0) {
            if (pipe->ssl[1]) {
            /* write some data */
               // DEBUGP (DDEBUG, "writeRegiionDataPipe", "sending data on SSL on ctx %lu", pipe->ssl[1]);
               if ((nwrite = I(SSLConnector)->write(pipe->ssl[1], spos, epos-spos)) < 0) {
                   DEBUGP (DERR,"writeRegiionDataPipe","%p - unable to write data to output pipe!",pipe);
                // fatal failure during sending data!
                return PIPE_FATAL;
               }
            } else {

            /* write some data */
            if ((nwrite = write (pipe->fds[1].fd, spos, epos-spos)) < 0) {
                DEBUGP (DERR,"writeDataPipe","%p - unable to write data to output pipe!",pipe);
                // fatal failure during sending data!
                return PIPE_FATAL;
            }
            }
            spos += nwrite;
            DEBUGP (DDEBUG,"writeDataPipe","%p - wrote %d bytes",pipe,nwrite);
        }

        DEBUGP (DDEBUG,"writeDataPipe","%p - sent all data from input -> output!",pipe);
        return PIPE_COMPLETE;
    }
    return PIPE_FATAL;
}

static int
flushDataPipe (data_pipe_t *pipe) {
    if (pipe) {
        while (TRUE) {
            int state;
            switch ((state = I (DataPipe)->poll (pipe, PIPE_POLLINOUT, PIPE_POLL_TIMEOUT))) {

              case PIPE_CONTINUE:
              case PIPE_BROKEN:
                  if (pipe->fds[0].revents & POLLIN) {
                      state = I (DataPipe)->read (pipe);
                      //polltype = PIPE_POLLINOUT;
                      pipe->fds[1].events = POLLOUT;
                  }
                  
                  if (pipe->fds[1].revents & POLLOUT) {
                      int res = I (DataPipe)->write (pipe);
                      if (state == PIPE_BROKEN && res == PIPE_COMPLETE) return PIPE_BROKEN;
                      state = res;
                  } else if (state == PIPE_BROKEN) {
                      DEBUGP (DWARN,"flushDataPipe","%p - input is broken, trying to flush remaining data...",pipe);
                      usleep (10000);
                      continue;
                  }
            }

            switch (state) {
              case PIPE_NEEDWRITE:
                  usleep (500);
                  
              case PIPE_CONTINUE:
                  continue;
            }
            
            return state;
        }
    }
    return PIPE_FATAL;
}

static void
cleanBeforeDataPipe (data_pipe_t *pipe, char *upto) {
    if (pipe) {
        if (!upto) upto = pipe->pos;
        if (upto < pipe->dataend) {
            DEBUGP (DDEBUG,"cleanBeforeDataPipe","moving data to beginning of buffer");
            memmove (pipe->buf, upto, pipe->dataend-upto);
            pipe->dataend = pipe->buf + (pipe->dataend-upto);
            pipe->pos = pipe->buf;
        } else {
            DEBUGP (DDEBUG,"cleanBeforeDataPipe","reset data pos to beginning of buffer");
            pipe->pos = pipe->dataend = pipe->buf;
        }
    }
}

static void
cleanDataPipe (data_pipe_t *pipe) {
    cleanBeforeDataPipe (pipe, pipe->marker);
}

static void
destroyDataPipe (data_pipe_t **ptr) {
    if (ptr) {
        data_pipe_t *pipe = *ptr;
        if (pipe) {
            if (_hasInitialized) {
                MODULE_LOCK ();
                if (_intercepts[pipe->fds[0].fd]) {
                    close (pipe->fds[0].fd);
                    _intercepts[pipe->fds[0].fd] = FALSE;
                }
                if (_intercepts[pipe->fds[1].fd]) {
                    close (pipe->fds[1].fd);
                    _intercepts[pipe->fds[1].fd] = FALSE;
                }
                pipe->ssl[0] = NULL;
                pipe->ssl[1] = NULL;
                MODULE_UNLOCK ();
            }
            
            free (pipe->buf);
            free (pipe);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (DataPipe) = {
    .new           = newDataPipe,
    .fill          = fillDataPipe,
    .hasData       = hasDataInPipe,
    .poll          = pollDataPipe,
    .read          = readDataPipe,
    .write         = writeDataPipe,
    .writeRegion   = writeRegionDataPipe,
    .flush         = flushDataPipe,
    .replaceInput  = replaceInput,
    .replaceOutput = replaceOutput,
    .setMaxSize    = setMaxSize,
    .setMarker     = setMarker,
    .clean         = cleanDataPipe,
    .cleanBefore   = cleanBeforeDataPipe,
    .reset         = resetDataPipe,
    .reverse       = reverseDataPipe,
    .destroy       = destroyDataPipe
};

/*//////// DataPipeStream Interface Implementation //////////////////////////////////////////*/

static data_pipe_stream_t *
newDataPipeStream (data_pipe_t *in2out, data_pipe_t *out2in) {
    /* XXX - hackish, I know... */
    if (!_hasInitialized) {
        memset (_intercepts,0,sizeof (_intercepts));
        _hasInitialized = TRUE;
    }

    if (in2out && out2in) {
        data_pipe_stream_t *stream = (data_pipe_stream_t *)calloc (1,sizeof (data_pipe_stream_t));
        if (stream) {
            stream->in2out = in2out;
            stream->out2in = out2in;
            stream->timeout = PIPESTREAM_POLL_TIMEOUT;
            stream->initiator = PIPESTREAM_UNKNOWN;
        }
        return stream;
    }
    return NULL;
}

static void
setTimeoutDataPipeStream (data_pipe_stream_t *stream, int timeout) {
    if (stream) {
        stream->timeout = timeout;
        DEBUGP (DDEBUG,"setTimeoutDataPipeStream","poll timeout set to %d msecs",timeout);
    }
}

static void
setInitiatorDataPipeStream (data_pipe_stream_t *stream, enum initiator initiator) {
    if (stream) {
        stream->initiator = initiator;
        DEBUGP (DDEBUG,"setInitiatorDataPipeStream","initiator set to %d",initiator);
    }
}

/*
 * polls the DataPipeStream to return available pipe for reading
 * 
 * we favor dealing with data from OUTPUT pipe first
 *
 * NOTE: this call will succeed EVEN IF one of the pipe is broken, as
 * long as there is data available in the pipe for reading or already
 * in the buffer. The higher layer call must deal with this scenario.
 * 
 * returns: ready DataPipe for reading or PIPESTREAM_UNKNOWN 
 */
static enum initiator
pollDataPipeStream (data_pipe_stream_t *stream, int timeout) {
    if (stream) {
        struct timeval tv1, tv2;
        struct pollfd fds[2];

        fds[0].fd = stream->in2out->fds[0].fd;
        fds[1].fd = stream->out2in->fds[0].fd;
        fds[0].events = POLLIN;
        fds[1].events = POLLIN;

        if (I (DataPipe)->hasData (stream->out2in)) return PIPESTREAM_OUT2IN;
        if (I (DataPipe)->hasData (stream->in2out)) return PIPESTREAM_IN2OUT;

        while (1) {
            gettimeofday (&tv1,NULL);
            switch (poll (fds, 2, timeout)) {
              case -1:
                  if (errno == EINTR && !SystemExit) {
                      if (timeout > 0) {
                          gettimeofday (&tv2,NULL);
                          timeout -= ((tv2.tv_sec - tv1.tv_sec) * 1000) + ((tv2.tv_usec - tv1.tv_usec) / 1000);
                          if (timeout > 0) continue;
                      } else continue;   
                  }
              case 0:
                  return PIPESTREAM_UNKNOWN;
            }

            if (fds[1].revents & POLLIN) {
                return PIPESTREAM_OUT2IN;
            }

            if (fds[0].revents & POLLIN) {
                return PIPESTREAM_IN2OUT;
            }


            if (fds[1].revents & POLLERR || fds[1].revents & POLLHUP || fds[1].revents & POLLNVAL) {
                DEBUGP (DWARN,"pollDataPipeStream","output pipe has issue!");
                return PIPESTREAM_UNKNOWN;
            }

            if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP || fds[0].revents & POLLNVAL) {
                DEBUGP (DWARN,"pollDataPipeStream","input pipe has issue!");
                return PIPESTREAM_UNKNOWN;
            }

            DEBUGP (DDEBUG,"pollDataPipeStream","how did we get here?");

            /*
             * don't care what the problem is, neither pipe is ready for reading
             */ 
            return PIPESTREAM_UNKNOWN;
        }
    }
    return PIPESTREAM_UNKNOWN;
}


/*
 * attempts to synchronize a DataPipeStream
 * a synchronization session is (1) complete in2out and (1) complete out2in operation
 *
 * returns: TRUE on success and has more, FALSE otherwise
 */
static boolean_t
syncDataPipeStream (data_pipe_stream_t *stream) {
    if (stream) {
        boolean_t syncIn2Out = FALSE;
        boolean_t syncOut2In = FALSE;
        
        int timeout = stream->timeout;
        
        struct timeval tv1, tv2;
        
        struct pollfd fds[2];
        fds[0].fd = stream->in2out->fds[0].fd;
        fds[1].fd = stream->out2in->fds[0].fd;

        fds[0].events = POLLIN;
        fds[1].events = POLLIN;

        if (I (DataPipe)->hasData (stream->in2out)) goto flush_c2s;
        if (I (DataPipe)->hasData (stream->out2in)) goto flush_s2c;
        
        while (1) {
            gettimeofday (&tv1,NULL);
            //DEBUGP (DDEBUG,"syncDataPipeStream","poll checking for %d and %d (%d ms timeout)",fds[0].fd,fds[1].fd,timeout);
            switch (poll (fds, 2, timeout)) {
              case -1:
                  if (errno == EINTR && !SystemExit) {
                      if (timeout > 0) {
                          gettimeofday (&tv2,NULL);
                          timeout -= ((tv2.tv_sec - tv1.tv_sec) * 1000) + ((tv2.tv_usec - tv1.tv_usec) / 1000);
                          if (timeout > 0) continue;
                      } else continue;   
                  }
              case 0: 
                DEBUGP (DDEBUG,"syncDataPipeStream","POLL EXIT -%d ",errno);
                return FALSE;
            }

            timeout = stream->timeout; /* reset timeout */
            
            /*
             * first, handle error cases related to INPUT & OUTPUT streams
             */
            if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP || fds[0].revents & POLLNVAL) {
                /*
                 * something's wrong with the input stream!
                 *
                 * we're going to assume end of stream, but first
                 * check if there's stuff to flush between input and
                 * output.
                 */
                DEBUGP (DWARN,"syncDataPipeStream","input socket has issue (probably closed), final flush from input -> output!");
                // XXX - mark that the input socket is dead!
                goto flush_c2s;
            }

            if (fds[1].revents & POLLERR || fds[1].revents & POLLHUP || fds[1].revents & POLLNVAL) {
                /*
                 * something's wrong with the output stream!
                 *
                 * we're going to assume end of stream, but first
                 * check if there's stuff to flush between input and
                 * output.
                 */
                DEBUGP (DWARN,"syncDataPipeStream","output socket has issue (probably closed), final flush from output -> input!");
                // XXX - mark that the output socket is dead!
                goto flush_s2c;
            }

            if (fds[0].revents & POLLIN) {
                if (stream->initiator == PIPESTREAM_IN2OUT && syncOut2In) {
                    DEBUGP (DDEBUG,"syncDataPipeStream","completed sync operation!");
                    return TRUE;
                }
                DEBUGP (DDEBUG,"syncDataPipeStream","something to read from input!");
                
              flush_c2s:
                switch (I (DataPipe)->flush (stream->in2out)) {
                  case PIPE_COMPLETE:
                      if (stream->initiator == PIPESTREAM_UNKNOWN) stream->initiator = PIPESTREAM_IN2OUT;
                      syncIn2Out = TRUE;
                      break;
                  case PIPE_BROKEN:
                      DEBUGP (DDEBUG,"syncDataPipeStream","stream broken!");
                      return FALSE;
                  case PIPE_NEEDWRITE:
                      goto flush_c2s;
                  case PIPE_FATAL:
                      return FALSE;
                }
            }

            if (fds[1].revents & POLLIN) {
                if (stream->initiator == PIPESTREAM_OUT2IN && syncIn2Out) {
                    DEBUGP (DDEBUG,"syncDataPipeStream","completed sync operation!");
                    return TRUE;
                }
                DEBUGP (DDEBUG,"syncDataPipeStream","something to read from output!");
                
              flush_s2c:
                switch (I (DataPipe)->flush (stream->out2in)) {
                  case PIPE_COMPLETE:
                      if (stream->initiator == PIPESTREAM_UNKNOWN) stream->initiator = PIPESTREAM_OUT2IN;
                      syncOut2In = TRUE;
                      break;
                  case PIPE_BROKEN:
                      DEBUGP (DDEBUG,"syncDataPipeStream","stream broken!");
                      return FALSE;
                  case PIPE_NEEDWRITE:
                      goto flush_s2c;
                  case PIPE_FATAL:
                      return FALSE;
                }
            }
        }
    }
    return FALSE;
}

static void
flushDataPipeStream (data_pipe_stream_t *stream) {
    if (stream) {
        int transactions = 0;
        while (syncDataPipeStream (stream)) {
            transactions++;
        }
        DEBUGP (DINFO,"flushDataPipeStream","flushed with %d sync transactions!",transactions);
    }
}

static void
resetDataPipeStream (data_pipe_stream_t *stream) {
    if (stream) {
        /* clean-up the stream pipe buffers! */
        I (DataPipe)->reset (stream->in2out);
        I (DataPipe)->reset (stream->out2in);
    }
}

static void
destroyDataPipeStream (data_pipe_stream_t **ptr) {
    if (ptr) {
        data_pipe_stream_t *stream = *ptr;
        if (stream) {

            I (DataPipe)->destroy (&stream->in2out);
            I (DataPipe)->destroy (&stream->out2in);

            free (stream);
            *ptr = NULL;
        }
    }
}

IMPLEMENT_INTERFACE (DataPipeStream) = {
    .new        = newDataPipeStream,
    .poll       = pollDataPipeStream,
    .sync       = syncDataPipeStream,
    .flush      = flushDataPipeStream,
    .initiator  = setInitiatorDataPipeStream,
    .setTimeout = setTimeoutDataPipeStream,
    .reset      = resetDataPipeStream,
    .destroy    = destroyDataPipeStream
};

/*//////// StreamIntercept Interface Implementation //////////////////////////////////////////*/

static stream_intercept_t *
newStreamIntercept (data_pipe_stream_t *target) {

    /* XXX - hackish... I know */
    if (!_hasInitialized) {
        memset (_intercepts,0,sizeof (_intercepts));
        _hasInitialized = TRUE;
    }
    
    if (target) {
        socket_pair_t *pair = I (SocketPair)->new ();
        if (pair) {
            stream_intercept_t *intercept = (stream_intercept_t *)calloc (1,sizeof (stream_intercept_t));
            if (intercept) {
                
                MODULE_LOCK ();
                _intercepts[pair->fds[0]] = TRUE;
                _intercepts[pair->fds[1]] = TRUE;
                MODULE_UNLOCK ();
        
                /*
                 * new socket pair A and B
                 *
                 * This intercept routine operates as follows... 
                 * 
                 * original in2out and out2in looked like this:
                 *   in2out = (client) -> (server)
                 *   out2in = (server) -> (client)
                 *
                 * new in2out and out2in looks like this:
                 *   in2out = (client) -> A
                 *   out2in = A -> (client)
                 *
                 * intercept in2out and out2in looks like this:
                 *   in2out = B -> (server)
                 *   out2in = (server) -> B
                 *
                 * in a nutshell, the pair A and B is acting as if the
                 * (client) is now speaking to A and the (server) is
                 * now speaking to B
                 *
                 * Since A == B is a socket pair pipe, the new
                 * intercept in essence is acting as if it is a proxy
                 * server to the (client)
                 *   
                 */

                intercept->pair = pair;
                
                /*
                 * take over original output-side (client->server) socket with new socket pipe A
                 * the original in2out pipe's output is now the output for new in2out intercept
                 *
                 * also, set the new input for the in2out intercept to be the new socket pipe B 
                 */
                intercept->in2out[1] = I (DataPipe)->replaceOutput (target->in2out,pair->fds[0]);
                intercept->in2out[0] = pair->fds[1];

                /*
                 * take over original input-side (server->client) socket with new socket pipe A
                 * the original out2in pipe's input is now the output for the new out2in intercept
                 * 
                 * also, set the new input for out2in intercept to be new socket pipe A
                 */
                intercept->out2in[0] = I (DataPipe)->replaceInput (target->out2in,pair->fds[0]);
                intercept->out2in[1] = pair->fds[1];

                DEBUGP (DDEBUG,"newStreamIntercept","created new intercepts for in2out: (%d -> %d == %d -> %d)",
                        target->in2out->fds[0].fd, target->in2out->fds[1].fd, intercept->in2out[0], intercept->in2out[1]);
                DEBUGP (DDEBUG,"newStreamIntercept","created new intercepts for out2in: (%d -> %d == %d -> %d)",
                        intercept->out2in[0], intercept->out2in[1], target->out2in->fds[0].fd, target->out2in->fds[1].fd);

                return intercept;
            }
        } else {
            DEBUGP (DERR,"newStreamIntercept","unable to create socket pairs for client and server intercepts!");
        }
        I (SocketPair)->destroy (&pair);
    }
    return NULL;
}

static void
closeStreamIntercept (stream_intercept_t *intercept) {
    if (intercept) {
        I (SocketPair)->close (intercept->pair);
    }
}

static void
destroyStreamIntercept (stream_intercept_t **ptr) {
    if (ptr) {
        stream_intercept_t *intercept = *ptr;
        if (intercept) {
            //I (SocketPair)->destroy (&intercept->pair);
            free (intercept->pair);
            free (intercept);
        }
    }
}

IMPLEMENT_INTERFACE (StreamIntercept) = {
    .new        = newStreamIntercept,
    .close      = closeStreamIntercept,
    .destroy    = destroyStreamIntercept
};

/*//////// Transformation Interface Implementation //////////////////////////////////////////*/

TRANSFORM_EXEC (streamflush) {
    data_pipe_stream_t *stream = (data_pipe_stream_t *)in->data;
    I (DataPipeStream)->flush (stream);
    return NULL;
}

TRANSFORM_EXEC (stream2intercept) {
    data_pipe_stream_t *stream = (data_pipe_stream_t *)in->data;
    stream_intercept_t *intercept = I (StreamIntercept)->new (stream);
    if (intercept) {
        transform_object_t *obj = I (TransformObject)->new (xform->to,intercept);
        if (obj) {
            obj->destroy = (XDESTROY) I (StreamIntercept)->destroy;
            return obj;
        }
        free (intercept);
    }
    return NULL;
}

TRANSFORM_EXEC (intercept2stream) {
    stream_intercept_t *intercept = (stream_intercept_t *)in->data;
    data_pipe_t *c2s = I (DataPipe)->new (intercept->in2out[0], intercept->in2out[1]);
    data_pipe_t *s2c = I (DataPipe)->new (intercept->out2in[0], intercept->out2in[1]);
    if (c2s && s2c) {
        data_pipe_stream_t *stream = I (DataPipeStream)->new (c2s,s2c);
        if (stream) {
            transform_object_t *obj = I (TransformObject)->new ("data:pipe:stream",stream);
            if (obj) {
                obj->destroy = (XDESTROY) I (DataPipeStream)->destroy;
                return obj;
            }
            I (DataPipeStream)->destroy (&stream);
        }
    }
    I (DataPipe)->destroy (&c2s);
    I (DataPipe)->destroy (&s2c);
    
    return NULL;
}

TRANSFORM_EXEC (stream2reqpipe) {
    data_pipe_stream_t *stream = (data_pipe_stream_t *)in->data;
    transform_object_t *obj = I (TransformObject)->new (xform->to,stream->in2out);
    if (obj) {
        stream->in2out = NULL;           /* deletes reference */
        obj->destroy = (XDESTROY) I (DataPipe)->destroy;
        return obj;
    }
    return NULL;
}

TRANSFORM_EXEC (stream2respipe) {
    data_pipe_stream_t *stream = (data_pipe_stream_t *)in->data;
    transform_object_t *obj = I (TransformObject)->new (xform->to,stream->out2in);
    if (obj) {
        stream->out2in = NULL;           /* deletes reference */
        obj->destroy = (XDESTROY) I (DataPipe)->destroy;
        return obj;
    }
    return NULL;
}

TRANSFORM_EXEC (pipeflush) {
    data_pipe_t *pipe = (data_pipe_t *)in->data;

    while (TRUE) {
        
        switch (I (DataPipe)->poll (pipe, PIPE_POLLIN, PIPE_POLL_TIMEOUT)) {
          case PIPE_FATAL: return NULL;
          case PIPE_TIMEOUT: continue;
        }
        
        /*
         * we try to flush until there is an error. 
         */
        switch (I (DataPipe)->flush (pipe)) {
          case PIPE_BROKEN:
          case PIPE_FATAL: return NULL;
              
        }
    }
    return NULL;
}

TRANSFORM_NEW (newDataPipeTransformation) {

    TRANSFORM ("data:pipe:stream", "data:pipe:stream@flush", streamflush);
    TRANSFORM ("data:pipe:stream", "data:pipe:intercept", stream2intercept);
    TRANSFORM ("data:pipe:intercept","data:pipe:stream", intercept2stream);

    TRANSFORM ("data:pipe:stream","data:pipe::request*", stream2reqpipe);
    TRANSFORM ("data:pipe:stream","data:pipe::response*", stream2respipe);

    TRANSFORM ("data:pipe","data:pipe@flush", pipeflush);
    TRANSFORM ("data:pipe::*","data:pipe@flush", pipeflush);
    
} TRANSFORM_NEW_FINALIZE;

TRANSFORM_DESTROY (destroyDataPipeTransformation) {


} TRANSFORM_DESTROY_FINALIZE;

IMPLEMENT_INTERFACE (Transformation) = {
	.new       = newDataPipeTransformation,
	.destroy   = destroyDataPipeTransformation,
	.execute   = NULL,
	.free      = NULL
};
