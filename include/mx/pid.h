
#ifndef __MX_PID_H_
#define __MX_PID_H_

int pidfile_open(const char *path, const char *name);
int pidfile_write(int fd, int pid);
int pidfile_close(int fd);
int pidfile_remove(int fd);


#endif /* __MX_PID_H_ */
