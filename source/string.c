
#include "mx/string.h"
#include "mx/memory.h"
#include "mx/log.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>





/**
 * Move string to another variable
 *
 */
char* xstrmove(char **str)
{
    char *tmp = *str;
    *str = NULL;
    return tmp;
}


/**
 *
 */
char* xstrcpy(char *dst, const char *src)
{
    const size_t len = strlen(src);
    return (char*)memcpy(dst, src, len+1) + len;
}


/**
 *
 */
char* xstrncpy(char *dst, const char *src, size_t maxlen)
{
    const char *p = memchr(src, '\0', maxlen);
    if (p) {
        size_t srclen = (size_t)(p - src);
        memcpy(dst, src, srclen);
        memset(dst+srclen, 0, maxlen - srclen);
        return dst+srclen;
    }
    else {
        memcpy(dst, src, maxlen);
        return dst+maxlen;
    }
}


/**
 *
 */
size_t xstrlcpy(char *dst, const char *src, size_t size)
{
    size_t ret = strlen(src);
    if (size) {
        size_t len = (ret >= size) ? size-1 : ret;
        memcpy(dst, src, len);
        dst[len] = '\0';
    }

    return ret;
}


/**
 *
 */
char* xstrdup(const char *str)
{
    return xstrdupex(NULL, str);
}


/**
 *
 */
char* xstrdupex(char *buffer, const char *str)
{
    buffer = xrealloc(buffer, strlen(str)+1);
    xstrcpy(buffer, str);
    return buffer;
}



/**
 *
 */
char* xstrndup(const char *str, size_t len)
{
    char *p = memchr(str, '\0', len);
    return xmemdupz(str, p ? (size_t)(p-str) : len);
}



/**
 * Convert string to number
 *
 */
bool xstrtol(const char *str, long *val, int base)
{
    long tmp;
    char *end = NULL;
    tmp = strtol(str, &end, base);
    if (*end != '\0') {
        ERROR("Could not convert value '%s' to long\n", str);
        return false;
    }
    *val = tmp;
    return true;
}



/**
 * Join strings
 *
 * Caller is responsible for freeing memory
 *
 */
char* xstrjoin(const char *first, const char *second, const char *third)
{
    return xstrjoinex(NULL, first, second, third);
}


/**
 * Join strings within given buffer
 *
 * Buffer has to be allocated, it will be resized if needed
 *
 */
char* xstrjoinex(char *buffer, const char *first, const char *second, const char *third)
{
    size_t buffer_len = strlen(first) + strlen(second) + 1;   // +1 null terminator
    if (third)
        buffer_len += strlen(third);

    buffer = xrealloc(buffer, buffer_len);
    char *tmp = buffer;
    tmp = xstrcpy(tmp, first);
    tmp = xstrcpy(tmp, second);
    if (third)
        xstrcpy(tmp, third);

    return buffer;
}



/**
 * Trim string from the left
 *
 */
char* xstrltrim(const char *str)
{
    while (*str && !isgraph(*str))
        str++;

    return (char*)str;
}


/**
 * Trim string from the right
 *
 */
char* xstrrtrim(char *str)
{
    for (size_t i=strlen(str)-1; i>0; i--) {
        if (isgraph(str[i]))
            break;
        str[i] = '\0';
    }

    return str;
}


/**
 * Check if string starts with prefix
 *
 */
bool xstrstarts(const char *str, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    return (strlen(str) < prefix_len) ? false : strncmp(str, prefix, prefix_len) == 0;
}


/**
 * Check if string ends with postfix
 *
 */
bool xstrends(const char *str, const char *postfix)
{
    size_t str_len = strlen(str);
    size_t postfix_len = strlen(postfix);
    return (str_len < postfix_len) ? false : strcmp(str+(str_len-postfix_len), postfix) == 0;
}





struct xstrtok
{
    const char *prev_chunk;
    char *chunk;
};


/**
 * Create new tokenizer instance.
 *
 * The 'original' string has to be valid during tokenizer life time, it is not copied
 * internally.
 *
 * The internal buffer is reused to store separate chunks.
 *
 */
struct xstrtok* xstrtok_new(const char *original)
{
    struct xstrtok *tkzr = xmalloc(sizeof(struct xstrtok));
    tkzr->prev_chunk = original;
    tkzr->chunk = NULL;
    return tkzr;
}


/**
 * Delete tokenizer instance.
 *
 */
struct xstrtok* xstrtok_delete(struct xstrtok *tkzr)
{
    if (tkzr->chunk)
        free(tkzr->chunk);
    free(tkzr);
    return NULL;
}


/**
 * Return next token.
 *
 */
const char* xstrtok_next(struct xstrtok *tkzr, const char *separators)
{
    if (!tkzr->prev_chunk)
        return NULL;    // Tokenizer empty

    const char *separator = NULL;
    // Find shortest chunk
    while (*separators) {
        const char *tmp = strchr(tkzr->prev_chunk, *separators);
        separators++;
        if (!tmp)               // Separator not available
            continue;
        if (!separator) {
            separator = tmp;    // First chunk was found
            continue;
        }
        if (tmp < separator)
            separator = tmp;    // Shorter chunk was found
    }

    const char *ret;
    if (separator) {
        size_t chunk_len = separator - tkzr->prev_chunk;
        tkzr->chunk = xrealloc(tkzr->chunk, chunk_len+1);
        memmove(tkzr->chunk, tkzr->prev_chunk, chunk_len);
        tkzr->chunk[chunk_len] = '\0';
        tkzr->prev_chunk = separator+1;
        ret = tkzr->chunk;
    }
    else {
        ret = tkzr->prev_chunk;
        tkzr->prev_chunk = NULL;
    }

    return ret;
}


/**
 * Return string word length
 *
 */
size_t xstr_word_len(const char *str, const char *space_chars)
{
    size_t idx = 0;
    char ch;

    while (ch = str[idx], ch) {
        if (space_chars == NULL) {
            // End with any whitespace character
            if (!isgraph(ch))
                return idx;
        }
        else {
            // End with defined space characters
            if (strchr(space_chars, ch))
                return idx;
        }

        idx++;
    }

    return idx;
}


/**
 * Return string space length
 *
 */
size_t xstr_space_len(const char *str, const char *space_chars)
{
    size_t idx = 0;
    char ch;

    while (ch = str[idx], ch) {
        if (space_chars == NULL) {
            // End with any non whitespace character
            if (isgraph(ch))
                return idx;
        }
        else {
            // Only defined space characters
            if (!strchr(space_chars, ch))
                return idx;
        }

        idx++;
    }

    return idx;
}
