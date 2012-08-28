#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Platform independent data serialization.",
	.requires = LIST ( "corenova.data.list"),
	.implements  = LIST ("StreamWriter", "StreamReader")
};

#include <corenova/data/streams.h>
#include <corenova/net/socket.h>

static void
_bufwrite(stream_handle_t *handle, void *data, int size) {

    int page_free = 0;
    int written = 0;
    int bytes_left = 0;
    
    if(handle->type != T_WRITE)
	return;

    while(written < size) {

	page_free = STREAM_PAGE_SIZE - handle->size % STREAM_PAGE_SIZE;
	bytes_left = size - written;	
	
	if(bytes_left < page_free) {
	
	    memcpy(handle->page + handle->offset, data + written, bytes_left);
	    handle->size += bytes_left;
	    handle->offset += bytes_left;
	    written += bytes_left;
 	    
	} else {
	
	    memcpy(handle->page + handle->offset, data + written, page_free);
	    written += page_free;
	    handle->size += page_free;
	    
	    handle->page = malloc(STREAM_PAGE_SIZE);
	    
	    I (List)->insert(handle->buffer, I (ListItem)->new(handle->page));
	    
	    handle->offset = 0;
	
	}
    
    }
    
}

static int
_bufread(stream_handle_t *handle, void *data, int size) {

    if(handle->type != T_READ)
	return 0;
	
    memcpy(data, handle->page + handle->offset, size);
    handle->offset += size;
    
    return size;

}

static void
_reset(stream_handle_t *handle) {

    if(handle->type != T_READ)
	return;

    handle->offset = 0;

}

static void
_append(stream_handle_t *handle, void *data, char type, int size) {

    if(handle->type != T_WRITE)
	return;
	
    // store platform in each variable's type field
	
    type |= (handle->local_platform << 4);
    
    _bufwrite(handle, &type, 1);

    switch(type & 0x0f) {
    
	case INT8:  
		    _bufwrite(handle, data, 1);
		    break;
	case INT16N:
	case INT16: 
		    _bufwrite(handle, data, 2);
		    break;
	case INT32N:
	case INT32: 
		    _bufwrite(handle, data, 4);
		    break;
	case INT64N:
	case INT64: 
		    _bufwrite(handle, data, 8);
		    break;
	case FLOAT8:
		    _bufwrite(handle, data, 1);
		    break;
	case FLOAT16: 
		    _bufwrite(handle, data, 2);
		    break;
	case FLOAT32:	
		    _bufwrite(handle, data, 4);
		    break;
	case FLOAT64:
		    _bufwrite(handle, data, 8);
		    break;
	case BINARY_STRING:
		    _bufwrite(handle, &size, 4);
		    _bufwrite(handle, data, size);
		    break;
	case STRING:
		    _bufwrite(handle, &size, 4);
		    _bufwrite(handle, data, size);
		    break;

    }

}

static void
_appendArray(stream_handle_t *handle, void **data, char type, int size) {

    int i = 0;

    while(data[i] != NULL) {
    
	_append(handle, data[i], type, size);
	i++;
    
    }

}

static void
_appendArray2(stream_handle_t *handle, void **data, char *types, int size) {

    int i = 0;

    while(data[i] != NULL) {
    
	_append(handle, data[i], types[i], size);
	i++;
    
    }

}

static int
_fetch(stream_handle_t *handle, void **data, char *type, int size) {

    char register _tmp = 0;
    
#define L16toB16(x) _tmp = x[0]; x[0] = x[1]; x[1] = _tmp;
#define L32toB32(x) _tmp = x[0]; x[0] = x[3]; x[3] = _tmp; \
		    _tmp = x[1]; x[1] = x[2]; x[2] = _tmp;
#define L64toB64(x) _tmp = x[0]; x[0] = x[7]; x[7] = _tmp; \
                    _tmp = x[1]; x[1] = x[6]; x[6] = _tmp; \
		    _tmp = x[2]; x[2] = x[5]; x[5] = _tmp; \
		    _tmp = x[3]; x[3] = x[4]; x[4] = _tmp

    int read = 0;
    int realsize = 0;
    char platform;
    char vtype;

    if(handle->type != T_READ)
	return 0;

    _bufread(handle, &vtype, 1);
    
    if(*data == NULL && size != 0)
	*data = malloc(size);
	
    platform = (vtype & 0xf0) >> 4;
    
    vtype &= 0x0f;
    
    // report variable type only if requested
    
    if(type != NULL)
	*type = vtype;
	
    switch(vtype) {
    
	case INT8:  
		    read += _bufread(handle, *data, 1);
		    break;
	case INT16: 

		    read += _bufread(handle, *data, 2);

		    if(platform != handle->local_platform) {
		    
			if(platform == L32 || platform == L64) {
			
			    if(handle->local_platform == B32 || handle->local_platform == B64)
				L16toB16(((char*)*data));
			
			} else {
			
			    if(handle->local_platform == L32 || handle->local_platform == L64)
				L16toB16(((char*)*data));
			
			}
		    
		    }
		    
		    break;
	case INT32: 
		    read += _bufread(handle, *data, 4);
		    
		    if(platform != handle->local_platform) {
		    
			if(platform == L32 || platform == L64) {
			
			    if(handle->local_platform == B32 || handle->local_platform == B64)
				L32toB32(((char*)*data));
			
			} else {
			
			    if(handle->local_platform == L32 || handle->local_platform == L64)
				L32toB32(((char*)*data));
			
			}
		    
		    }

		    break;
	case INT64: 
		    read += _bufread(handle, *data, 8);

		    if(platform != handle->local_platform) {
		    
			if(platform == L32 || platform == L64) {
			
			    if(handle->local_platform == B32 || handle->local_platform == B64)
				L64toB64(((char*)*data));
			
			} else {
			
			    if(handle->local_platform == L32 || handle->local_platform == L64)
				L64toB64(((char*)*data));
			
			}
		    
		    }

		    break;
	case FLOAT8:
		    read += _bufread(handle, *data, 1);
		    break;
	case FLOAT16: 
		    read += _bufread(handle, *data, 2);
		    break;
	case FLOAT32:	
		    read += _bufread(handle, *data, 4);
		    break;
	case FLOAT64:
		    read += _bufread(handle, *data, 8);
		    break;
	case BINARY_STRING:

		    read += _bufread(handle, (void *)&realsize, 4);

		    if(platform != handle->local_platform) {
		    
			if(platform == L32 || platform == L64) {
			
			    if(handle->local_platform == B32 || handle->local_platform == B64)
				L32toB32(((char*)&realsize));
			
			} else {
			
			    if(handle->local_platform == L32 || handle->local_platform == L64)
				L32toB32(((char*)&realsize));
			
			}
		    
		    }
		    
		    if(*data == NULL)
			*data = malloc(realsize);
		    
		    read += _bufread(handle, *data, realsize);
		    break;
	case STRING:

		    read += _bufread(handle, (void *)&realsize, 4);

		    if(platform != handle->local_platform) {
		    
			if(platform == L32 || platform == L64) {
			
			    if(handle->local_platform == B32 || handle->local_platform == B64)
				L32toB32(((char*)&realsize));
			
			} else {
			
			    if(handle->local_platform == L32 || handle->local_platform == L64)
				L32toB32(((char*)&realsize));
			
			}
		    
		    }
		    
		    if(*data == NULL)
			*data = malloc(realsize+1);
			
		    ((char *)*data)[realsize] = '\0';
			
		    read += _bufread(handle, *data, realsize);
		    break;

	case INT16N:
		    read += _bufread(handle, *data, 2);
		    break;
	case INT32N:
		    read += _bufread(handle, *data, 4);
		    break;
	case INT64N:
		    read += _bufread(handle, *data, 8);
		    break;
        
    }
    
    return read;

}

static int
_fetchArray(stream_handle_t *handle, void **data, char *type, int size) {

    int i = 0;
    int result = 0;

    while(data[i] != NULL) {
    
	result+=_fetch(handle, &data[i], type, size);
	i++;
    
    }
    
    return result;
    
}

static stream_handle_t *
_newOutputStream(void) {

    int pcheck = 1;
    char *pp = (char *)&pcheck;

    stream_handle_t *os = (stream_handle_t *)calloc (1,sizeof (stream_handle_t));
    
    if (os) {
    
	os->type = T_WRITE;
	os->page = malloc(STREAM_PAGE_SIZE);
	os->buffer = I (List)->new();
	os->offset = 0;
	    
	I (List)->insert(os->buffer, I (ListItem)->new(os->page));

	switch(sizeof(pcheck)) {
	    
	    case 4: if(*pp == 1)
			os->local_platform = L32;
		    else
		        os->local_platform = B32;
		    break;
			
	    case 8: if(*pp == 1)
			os->local_platform = L64;
		    else
			os->local_platform = B64;
		    break;	    
	    	    
	}
	    
//	_append(os, &os->local_platform, INT8, 0);
		
    }
	
    return os;

}

static stream_handle_t *
_newInputStream(void *data, int size) {

    int pcheck = 1;
    char *pp = (char *)&pcheck;

    stream_handle_t *is = (stream_handle_t *)calloc (1,sizeof (stream_handle_t));
	
    if (is) {
    
	is->type = T_READ;
	is->page = data;
	is->size = size;
	is->offset = 0;
	
        switch(sizeof(pcheck)) {
	    
	    case 4: if(*pp == 1)
			is->local_platform = L32;
		    else
			is->local_platform = B32;
		    break;
			
	    case 8: if(*pp == 1)
			is->local_platform = L64;
		    else
			is->local_platform = B64;
		    break;	    
	    	    
	    }
		
    }

    return is;

}

static stream_handle_t *
_fromBinary(binary_t *binary) {

    if(binary) {
    
	return _newInputStream(binary->data, binary->size);
    
    }
    
    return NULL;

}

static void
_destroyInputStream(stream_handle_t **ptr) {

    if (ptr) {
	
	stream_handle_t *p = *ptr;

	if (p) {

	    free (p);
	    *ptr = NULL;
			
	}
	
    }
	
}

static void
_destroyOutputStream(stream_handle_t **ptr) {

    if (ptr) {
	
	stream_handle_t *p = *ptr;

	if (p) {
	
	    if(p->buffer) {
	
		list_item_t *item = NULL;
		
		while((item = I(List)->pop(p->buffer))!=NULL) {
		
		    free(item->data);
		    I(ListItem)->destroy(&item);
		    
		}
		
	    }
	    
	    I(List)->destroy(&(p->buffer));

	    free (p);
	    *ptr = NULL;
			
	}
	
    }
	
}

static void *
_serialize(stream_handle_t *handle) {

    int size = handle->size;
    int offset = 0;

    if(handle->type != T_WRITE)
	return NULL;

    void *buf = malloc(handle->size);
    
    list_item_t *iter = I (List)->first(handle->buffer);
    
    while(size > 0 && iter != NULL) {
    
	if(size > STREAM_PAGE_SIZE) {
	
	    memcpy(buf + offset, iter->data, STREAM_PAGE_SIZE);
	    size -= STREAM_PAGE_SIZE;
	    offset += STREAM_PAGE_SIZE;
		
	} else {
	
	    memcpy(buf + offset, iter->data, size);
	    size = 0;
	    offset += size;
	
	}
	
	iter = I (List)->next(iter);
    
    }
    
    return buf;

}

static binary_t*
_toBinary(stream_handle_t *handle, char *format) {

    if(!handle || !format)
	return NULL;

    binary_t *result = malloc(sizeof(binary_t));
    
    if(result) {
    
	result->size = handle->size;
	result->format = strdup(format);
	result->data = _serialize(handle);
    
    }
    
    return result;

}

static int
_write(stream_handle_t *handle, int fd) {

    return 0;

}

static int
_read(stream_handle_t *handle, int fd, int size) {

    return 0;

}

IMPLEMENT_INTERFACE (StreamWriter) = {
	.new     = _newOutputStream,
	.destroy = _destroyOutputStream,
	.append  = _append,
	.appendArray = _appendArray,
	.appendArray2 = _appendArray2,
	.serialize = _serialize,
	.toBinary = _toBinary,
	.write = _write
};

IMPLEMENT_INTERFACE (StreamReader) = {
	.new     = _newInputStream,
	.fromBinary = _fromBinary,
	.destroy = _destroyInputStream,
	.fetch = _fetch,
	.fetchArray = _fetchArray,
	.reset = _reset,
	.read = _read
};

