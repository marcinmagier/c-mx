
#ifndef __MX_IDLER_H_
  #define __MX_IDLER_H_

#include <stddef.h>
#include <stdbool.h>


#define STREAM_INCOMING_READY       (0x1 << 0)
#define STREAM_OUTGOING_READY       (0x1 << 1)



struct stream;


enum idler_status_e
{
    IDLER_ERROR = -2,
    IDLER_INTERRUPT = -1,
    IDLER_TIMEOUT = 0,
    IDLER_OPERATION,
};


struct idler;

struct idler* idler_new(void);
struct idler* idler_delete(struct idler *self);


void idler_add_stream(struct idler *self, struct stream *stream);
void idler_remove_stream(struct idler *self, struct stream *stream);
struct stream* idler_find_stream(struct idler *self, int fd);

int idler_wait(struct idler *self, unsigned long time_ms);

unsigned int idler_get_stream_status(struct idler *self, struct stream *stream);

struct stream* idler_get_next_stream(struct idler *self, struct stream *prev, unsigned int *status);


#endif /* __MX_IDLER_H_ */
