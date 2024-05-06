
#ifndef __MX_HTTP_H_
#define __MX_HTTP_H_

#include "mx/queue.h"
#include "mx/buffer.h"

#include <stdbool.h>
#include <stddef.h>



#define HTTP_LINE_END       "\r\n"
#define HTTP_HEADERS_END    "\r\n\r\n"



struct http_msg_view
{
    const char *method;
    const char *uri;
    const char *version;
    const char *status;
    const char *reason;
    const char *headers;
    const char *body;
};


bool http_msg_view_parse_request(struct http_msg_view *self, const char *buffer);
bool http_msg_view_parse_response(struct http_msg_view *self, const char *buffer);

const char* http_header_find(const char *headers, const char *name, const char **value);





struct http_header
{
    char *name;
    char *value;

    LIST_ENTRY(http_header) _entry_;
};

struct http_header* http_header_new(const char *name, const char *value);
struct http_header* http_header_delete(struct http_header *self);


// struct http_header_list
LIST_HEAD(http_header_list, http_header);





struct http_msg
{
    char *method;
    char *uri;
    char *version;
    unsigned int status;
    char *reason;

    struct http_header_list headers;
    struct buffer *body;
};

void http_msg_init(struct http_msg *self);
void http_msg_clean(struct http_msg *self);

struct http_msg* http_msg_new_request(const char *method, const char *uri, const char *version);
struct http_msg* http_msg_new_response(const char *version, unsigned int status, const char *reason);
struct http_msg* http_msg_delete(struct http_msg *self);

struct http_msg* http_msg_add_header(struct http_msg *self, const char *name, const char *value);
struct http_msg* http_msg_remove_header(struct http_msg *self, const char *name);

struct http_msg* http_msg_append_body(struct http_msg *self, const char *buffer, size_t length);

int http_msg_parse_request(struct http_msg *self, char *buffer);
int http_msg_parse_response(struct http_msg *self, char *buffer);
int http_msg_serialize(struct http_msg *self, char *buffer, size_t buffer_len);



#endif /* __MX_HTTP_H_ */
