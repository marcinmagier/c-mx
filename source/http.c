
#include "mx/http.h"
#include "mx/memory.h"
#include "mx/string.h"
#include "mx/misc.h"

#include <string.h>
#include <stdio.h>



#define HTTP_WHITESPACE_CHARS   " \t"


#define HTTP_REQ_LINE_FMT       "%s %s %s\r\n"      // METHOD   URI     VERSION
#define HTTP_RESP_LINE_FMT      "%s %u %s\r\n"      // VERSION  STATUS  REASON
#define HTTP_HEADER_LINE_FMT    "%s: %s\r\n"        // NAME: VALUE
#define HTTP_BODY_FMT           "%s"                // BODY




static bool http_view_parse(const char *buffer, const char **methver, const char **uristatus, const char **verreas,
                                                const char **headers, const char **body)
{
    char *headers_end = strstr(buffer, HTTP_HEADERS_END);
    if (headers_end == NULL)
        return false;   // More data is needed

    *body = headers_end + strlen(HTTP_HEADERS_END);
    *headers = strstr(buffer, HTTP_LINE_END) + strlen(HTTP_LINE_END);

    const char *first = xstrltrim(buffer);
    const char *second = NULL;
    const char *third = NULL;

    second = strchr(first, ' ');
    if (second) {
        second = xstrltrim(second);
        third = strchr(second, ' ');
    }
    if (third) {
        third = xstrltrim(third);
    }

    *methver = first;
    *uristatus = second;
    *verreas = third;

    return true;
}


bool http_msg_view_parse_request(struct http_msg_view *self, const char *buffer)
{
    self->status = NULL;
    self->reason = NULL;
    return http_view_parse(buffer, &self->method, &self->uri, &self->version, &self->headers, &self->body);
}


bool http_msg_view_parse_response(struct http_msg_view *self, const char *buffer)
{
    self->method = NULL;
    self->uri = NULL;
    return http_view_parse(buffer, &self->version, &self->status, &self->reason, &self->headers, &self->body);
}


const char* http_header_find(const char *headers, const char *name, const char **value)
{
    const char *line_start = headers;
    const char *line_end;
    const char *colon;
    size_t name_len = strlen(name);
    size_t line_len;

    do {
        line_start = line_start + xstr_space_len(line_start, HTTP_WHITESPACE_CHARS);
        line_end = strstr(line_start, HTTP_LINE_END);
        if (!line_end)
            break;  // Error
        line_len = line_end - line_start;
        if (line_len == 0)
            break;  // End of headers

        if (!strncasecmp(line_start, name, name_len)) {
            colon = line_start + name_len;
            colon += xstr_space_len(colon, HTTP_WHITESPACE_CHARS);
            if (*colon != ':')
                break; // Error

            if (value) {
                *value = colon + 1;
                *value += xstr_space_len(*value, HTTP_WHITESPACE_CHARS);
            }
            return line_start;
        }

        line_start = line_end + strlen(HTTP_LINE_END);
    } while (1);

    return NULL;
}



struct http_header* http_header_new(const char *name, const char *value)
{
    struct http_header *self = xmalloc(sizeof(struct http_header));

    self->name = xstrdup(name);
    self->value = value ? xstrdup(value) : NULL;

    return self;
}


struct http_header* http_header_delete(struct http_header *self)
{
    if (self->name)
        self->name = xfree(self->name);
    if (self->value)
        self->value = xfree(self->value);

    return xfree(self);
}




void http_msg_init(struct http_msg *self)
{
    memset(self, 0, sizeof(struct http_msg));
}


void http_msg_clean(struct http_msg *self)
{
    if (self->method)
        self->method = xfree(self->method);
    if (self->uri)
        self->uri = xfree(self->uri);
    if (self->version)
        self->version = xfree(self->version);
    if (self->reason)
        self->reason = xfree(self->reason);
    if (self->body)
        self->body = buffer_delete(self->body);

    struct http_header *header, *tmp;
     LIST_FOREACH_SAFE(header, &self->headers, _entry_, tmp) {
         LIST_REMOVE(header, _entry_);
         http_header_delete(header);
     }
}


struct http_msg* http_msg_new_request(const char *method, const char *uri, const char *version)
{
    struct http_msg *self = xmalloc(sizeof(struct http_msg));
    http_msg_init(self);

    self->method = xstrdup(method);
    self->uri = xstrdup(uri);
    self->version = xstrdup(version);

    LIST_INIT(&self->headers);

    return self;
}


struct http_msg* http_msg_new_response(const char *version, unsigned int status, const char *reason)
{
    struct http_msg *self = xmalloc(sizeof(struct http_msg));
    http_msg_init(self);

    self->version = xstrdup(version);
    self->status = status;
    self->reason = xstrdup(reason);

    LIST_INIT(&self->headers);

    return self;
}


struct http_msg* http_msg_delete(struct http_msg *self)
{
    http_msg_clean(self);
    return xfree(self);
}


struct http_msg* http_msg_add_header(struct http_msg *self, const char *name, const char *value)
{
    struct http_header *header = http_header_new(name, value);
    LIST_INSERT_HEAD(&self->headers, header, _entry_);

    return self;
}


struct http_msg* http_msg_remove_header(struct http_msg *self, const char *name)
{
    struct http_header *header, *tmp;
     LIST_FOREACH_SAFE(header, &self->headers, _entry_, tmp) {
         if (!strcasecmp(header->name, name)) {
             LIST_REMOVE(header, _entry_);
             http_header_delete(header);
         }
     }

     return self;
}


struct http_msg* http_msg_append_body(struct http_msg *self, const char *buffer, size_t length)
{
    if (self->body == NULL)
        self->body = buffer_new();

    buffer_append(self->body, buffer, length);
    return self;
}


int http_msg_parse_headers(struct http_msg *self, const char *headers)
{
    int ret = -1;
    size_t line_len;
    const char *line_start = headers;
    const char *line_end;

    do {
        line_start = line_start + xstr_space_len(line_start, HTTP_WHITESPACE_CHARS);
        line_end = strstr(line_start, HTTP_LINE_END);
        if (!line_end)
            break;  // Error
        line_len = line_end - line_start;
        if (line_len == 0) {
            ret = 1;
            break;  // End of headers, success
        }

        char buffer[line_len+1];
        memcpy(buffer, line_start, line_len);
        buffer[line_len] = '\0';

        char *colon = strchr(buffer, ':');
        if (colon == NULL)
            break;  // Error

        *colon = '\0';  // End header name
        char *name = xstrrtrim(buffer);
        char *value = xstrrtrim(xstrltrim(colon+1));
        http_msg_add_header(self, name, value);

        line_start = line_end + strlen(HTTP_LINE_END);
    } while (1);

    return ret;
}


int http_msg_parse_request(struct http_msg *self, char *buffer)
{
    struct http_msg_view http_view;
    if (!http_msg_view_parse_request(&http_view, buffer))
        return 0;   // More data needed

    size_t len;

    len = xstr_word_len(http_view.method, NULL);
    if (len == 0)
        return -1;  // Error
    self->method = xstrndup(http_view.method, len);

    len = xstr_word_len(http_view.uri, NULL);
    if (len == 0)
        return -1;  // Error
    self->uri = xstrndup(http_view.uri, len);

    len = xstr_word_len(http_view.version, NULL);
    if (len == 0)
        return -1;  // Error
    self->version = xstrndup(http_view.version, len);

    if (http_msg_parse_headers(self, http_view.headers) < 0)
        return -1;

    len = strlen(http_view.body);
    if (len > 0)
        http_msg_append_body(self, http_view.body, len);

    return 1;
}


int http_msg_parse_response(struct http_msg *self, char *buffer)
{
    struct http_msg_view http_view;
    if (!http_msg_view_parse_response(&http_view, buffer))
        return 0;   // More data needed

    size_t len;

    len = xstr_word_len(http_view.version, NULL);
    if (len == 0)
        return -1;  // Error
    self->version = xstrndup(http_view.version, len);

    len = xstr_word_len(http_view.status, NULL);
    if (len == 0)
        return -1;  // Error
    char status_str[len+1];
    memcpy(status_str, http_view.status, len);
    status_str[len] = '\0';
    long status_code;
    if (!xstrtol(status_str, &status_code, 10))
        return -1;  // Error
    self->status = status_code;

    len = xstr_word_len(http_view.reason, NULL);
    if (len == 0)
        return -1;  // Error
    self->reason = xstrndup(http_view.reason, len);

    if (http_msg_parse_headers(self, http_view.headers) < 0)
        return -1;

    len = strlen(http_view.body);
    if (len > 0)
        http_msg_append_body(self, http_view.body, len);

    return 1;
}


int http_msg_serialize(struct http_msg *self, char *buffer, size_t buffer_len)
{
    int len = 0;
    size_t offset = 0;

    // Serialize first line
    if (self->method && self->uri) {
        len = strlen(HTTP_REQ_LINE_FMT) + strlen(self->method) + strlen(self->uri) + strlen(self->version);
        if (len >= (int)buffer_len)
            return -1;
        len = snprintf(buffer+offset, buffer_len, HTTP_REQ_LINE_FMT, self->method, self->uri, self->version);
    }
    else {
        len = strlen(HTTP_RESP_LINE_FMT) + strlen(self->version) + 3 + strlen(self->reason);
        if (len >= (int)buffer_len)
            return -1;
        len = snprintf(buffer+offset, buffer_len, HTTP_RESP_LINE_FMT, self->version, self->status, self->reason);
    }

    offset += len;
    buffer_len -= len;

    // Serialize headers
    struct http_header *header;
    LIST_FOREACH(header, &self->headers, _entry_) {
        len = strlen(HTTP_HEADER_LINE_FMT) + strlen(header->name) + (header->value ? strlen(header->value) : 0);
        if (len >= (int)buffer_len)
            return -1;
        len = snprintf(buffer+offset, buffer_len, HTTP_HEADER_LINE_FMT, header->name, (header->value ? header->value : ""));

        offset += len;
        buffer_len -= len;
    }

    // Serialize end of headers
    len = strlen(HTTP_LINE_END);
    if (len >= (int)buffer_len)
        return -1;

    memcpy(buffer+offset, HTTP_LINE_END, len);
    offset += len;
    buffer_len -= len;

    // Serialize body
    if (self->body) {
        buffer_append(self->body, "\0", 1);    // Make sure string is properly ended
        len = strlen(HTTP_BODY_FMT) + self->body->length;
        if (len >= (int)buffer_len)
            return -1;
        len = snprintf(buffer+offset, buffer_len, HTTP_BODY_FMT, (char*)self->body->data);

        offset += len;
    }

    return offset;
}
