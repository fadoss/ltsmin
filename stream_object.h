

/** @file stream_object.h
 * @brief C encoding of the stream object.
 */

#ifndef STREAM_OBJECT_H
#define STREAM_OBJECT_H

#include "stream.h"

struct stream_obj {
	size_t(*read_max)(stream_t stream,void*buf,size_t count);
	void(*read)(stream_t stream,void*buf,size_t count);
	int(*empty)(stream_t stream);
	void(*write)(stream_t stream,void*buf,size_t count);
	void(*flush)(stream_t stream);
	void(*close)(stream_t *stream);
};

extern void stream_illegal_op(stream_t *stream);

extern void stream_illegal_void(stream_t stream);

extern int stream_illegal_int(stream_t stream);

extern void stream_illegal_io_op(stream_t stream,void*buf,size_t count);

extern size_t stream_illegal_io_try(stream_t stream,void*buf,size_t count);

#endif

