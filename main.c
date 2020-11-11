#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <nsenter.h>

char* get_systemd_pid(char* buff) {
    FILE* pipe_fp;
    int len;
    char data[64];
    int systemd_pid = 0;

    // TODO:  Potenial security vuln
    if ((pipe_fp = popen("/usr/bin/pgrep -o systemd$", "r")) == NULL) {
        perror("popen");
        return NULL;
    }

    // reading from the pipe
    if (fgets(data, 64, pipe_fp) == NULL) {
        perror("fgets");
        return NULL;
    }

    pclose(pipe_fp);

    systemd_pid = atoi(data);
    if (systemd_pid < 1 || systemd_pid > 4194304) {
        return NULL;
    }

    sprintf(buff, "%i", systemd_pid);

    return buff;
}

int main(int argc, char *argv[]) {
    // Arguments
    char nsenter[] = "/usr/bin/nsenter";
    // max pid on linux is 4,194,304, 7 digits long, plus 2 for '-t', plus 1 for '\0'
    char _t_systemd_pid[10] = "-t11";
    // max path provided by PATH_MAX includes '\0', plus 2 for '-w'
    char _w_cwd[2 + PATH_MAX] = "-w/";
    // uid is an unsigned 32 bit int, so the maximum is 4,294,967,295, 10 digits long, plus 2 for '-S', plus 1 for '\0'
    char _S_uid[13] = "-S0";
    // gid is an unsigned 32 bit int, so the maximum is 4,294,967,295, 10 digits long, plus 2 for '-S', plus 1 for '\0'
    char _G_gid[13] = "-G0";

    char* args[] = {
        nsenter,
        _t_systemd_pid,
        _w_cwd,
        _S_uid,
        _G_gid,
        "-a",
        (char*)NULL
    };

    // get current directory
    if (getcwd(&_w_cwd[2], PATH_MAX) == NULL) {
        err(EXIT_FAILURE, "Unable to determine cwd: %m");
    }

    // get uid and gid
    sprintf(_S_uid, "-S%u", getuid());
    sprintf(_G_gid, "-G%u", getgid());

    // get pid of systemd
    if (get_systemd_pid(&_t_systemd_pid[2]) == NULL) {
        err(EXIT_FAILURE, "Unable to find systemd: %m");
    }

    // Switch namespace
    // TODO:  Potential security vuln
    execv(nsenter, args);

    err(EXIT_FAILURE, "Entering systemd namespace failed!");
}