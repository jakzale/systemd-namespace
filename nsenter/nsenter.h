#ifndef SYSTEMD_WSL_NSENTER_H
#define SYSTEMD_WSL_NSENTER_H

#include <sys/types.h>

extern void
systemd_nsenter(pid_t systemd_pid, char* wd_path);

#endif