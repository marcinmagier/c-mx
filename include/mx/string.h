
#ifndef __MX_STRING_H_
#define __MX_STRING_H_


#include <stddef.h>
#include <stdbool.h>
#include <string.h>


char* xstrmove(char **str);

char* xstrcpy(char *dst, const char *src);
char* xstrncpy(char *dst, const char *src, size_t maxlen);
size_t xstrlcpy(char *dst, const char *src, size_t size);

char* xstrdup(const char *str);
char* xstrdupex(char *buffer, const char *str);
char* xstrndup(const char *str, size_t len);

bool xstrtol(const char *str, long *val, int base);

char* xstrjoin(const char *first, const char *second, const char *third);
char* xstrjoinex(char *buffer, const char *first, const char *second, const char *third);

char* xstrltrim(const char *str);
char* xstrrtrim(char *str);

bool xstrstarts(const char *str, const char *prefix);
bool xstrends(const char *str, const char *postfix);

struct xstrtok;
struct xstrtok* xstrtok_new(const char *str);
struct xstrtok* xstrtok_delete(struct xstrtok *tkzr);
const char* xstrtok_next(struct xstrtok *tkzr, const char *separators);

size_t xstr_word_len(const char *str, const char *space_chars);
size_t xstr_space_len(const char *str, const char *space_chars);

#endif /* __MX_STRING_H_ */
