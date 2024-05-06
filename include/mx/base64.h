
#ifndef __MX_BASE64_H_
#define __MX_BASE64_H_



#include <stddef.h>
#include <stdbool.h>



size_t base64_encode(const unsigned char *src, size_t len, unsigned char *out, size_t out_len, bool line_feed);
size_t base64_decode(const unsigned char *src, size_t len, unsigned char *out, size_t out_len);



#endif /* __MX_BASE64_H_ */
