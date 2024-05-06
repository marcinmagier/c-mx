
#include "mx/rand.h"
#include "mx/log.h"

#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/random.h>




/**
 * Open random device.
 *
 * Returns file description of the device.
 *
 */
int rand_dev_open(const char *device)
{
    int fd = open(device, O_WRONLY);

    if (fd < 0) {
        RESET("Cannot open '%s'", device);
    }

    return fd;
}


/**
 * Close random device.
 *
 */
void rand_dev_close(int fd)
{
    close(fd);
}


/**
 *  Add entropy data to random device.
 *
 *  Argument 'entropy_count' will update available entropy number - /proc/sys/kernel/random/entropy_avail
 *
 */
void rand_dev_add_entropy(int fd, const char *data, unsigned int len, unsigned int entropy_count)
{
    struct rand_pool_info *rpi;

    rpi = (struct rand_pool_info*) malloc(sizeof(struct rand_pool_info) + len);
    if (!rpi) {
        RESET("Out of memory");
    }

    rpi->entropy_count = entropy_count;
    rpi->buf_size = len;
    memcpy( &(rpi->buf[0]), data, len);

    int ret = ioctl(fd, RNDADDENTROPY, rpi);
    if (ret < 0) {
        ERROR("Cannot add entropy");
    }

    free(rpi);
}





/**
 * Fill buffer with random data
 *
 */
void rand_data(unsigned char *data, size_t len)
{
    for (size_t i=0; i<len; i++)
        data[i] = (unsigned char)(rand() & 0xFF);
}





/**
 *  Increase time with randomness
 *
 * Lowest progress   -  1  2  4  7  11  17  26  40   61   92  139  209  314   472
 *
 * Highest progress  -  1  2  4  8  15  26  46  81  142  249  436  764  1238  2167
 *
 */
unsigned int rand_time_increase(unsigned int *val, unsigned int max)
{
    unsigned int tmp = *val;
    if (max && tmp >= max)
        return max;

    tmp = tmp + (tmp+2)/2;
    if (tmp > 4)
        tmp += rand() % ((*val/4)+1);

    if (max && tmp >= max)
        tmp = max;

    *val = tmp;
    return tmp;
}


/**
 *  Decrease time
 *
 */
unsigned int rand_time_decrease(unsigned int *val)
{
    if (*val > 0)
        *val /= 2;
    return *val;
}
