
#ifndef __MX_RAND_H_
#define __MX_RAND_H_


#include <stddef.h>



#define RAND_DEV_RANDOM     "/dev/random"
#define RAND_DEV_URANDOM    "/dev/urandom"



int  rand_dev_open(const char *device);
void rand_dev_close(int fd);
void rand_dev_add_entropy(int fd, const char *data, unsigned int len, unsigned int entropy_count);

void rand_data(unsigned char *data, size_t len);

unsigned int rand_time_increase(unsigned int *val, unsigned int max);
unsigned int rand_time_decrease(unsigned int *val);



#endif /* __MX_RAND_H_ */
