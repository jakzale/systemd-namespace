#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <nsenter.h>

pid_t get_systemd_pid() {
    FILE* pipe_fp;
    int len;
    char data[64];
    pid_t systemd_pid = 0;

    // TODO:  Potenial security vuln
    if ((pipe_fp = popen("/usr/bin/pgrep -o systemd$", "r")) == NULL) {
        perror("popen");
        return -1;
    }

    // reading from the pipe
    if (fgets(data, 64, pipe_fp) == NULL) {
        perror("fgets");
        return -1;
    }

    pclose(pipe_fp);

    systemd_pid = atoi(data);

    if (systemd_pid < 1 || systemd_pid > 4194304) {
        return -1;
    }

    return systemd_pid;
}

int main(int argc, char *argv[]) {
    pid_t systemd_pid;
    char cwd[PATH_MAX];

    // get pid of systemd
    if ((systemd_pid = get_systemd_pid()) < 0) {
        err(EXIT_FAILURE, "Unable to find systemd: %m");
    }

    // get current directory
    if (getcwd(cwd, PATH_MAX) == NULL) {
        err(EXIT_FAILURE, "Unable to determine cwd: %m");
    }

    // Switch namespace
    systemd_nsenter(systemd_pid, cwd);

    err(EXIT_FAILURE, "Entering systemd namespace failed!");
}