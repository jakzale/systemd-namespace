#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <bits/local_lim.h>

#include <nsenter.h>
#include <proc/readproc.h>

#include <security/pam_appl.h>

pid_t get_systemd_pid() {
    PROCTAB *PT;
    proc_t task;
    pid_t pid = -1;
    unsigned long long start_time = ~0ULL;

    PT = openproc(PROC_FILLSTATUS);

    memset(&task, 0, sizeof(task));
    while (readproc(PT, &task)) {
        char* cmd = task.cmd;
        if (strcmp(cmd, "systemd") == 0 && task.start_time < start_time) {
            pid = task.tid;
            start_time = task.start_time;
        }
    }

    closeproc(PT);

    return pid;
}

/*
void login_user(char* username)
{
    int rc;
    // TODO:  verify that service name is correct
    char service_name[] = "login";
    pam_handle_t *pamh = NULL;

    rc = pam_start(service_name, username, NULL, &pamh);

}
*/

int main(int argc, char *argv[]) {
    pid_t systemd_pid;
    char cwd[PATH_MAX];

    /* Get pid of the oldest systemd process */
    if ((systemd_pid = get_systemd_pid()) < 0) {
        err(EXIT_FAILURE, "Unable to find systemd");
    }

    // get current directory
    if (getcwd(cwd, PATH_MAX) == NULL) {
        err(EXIT_FAILURE, "Unable to determine cwd");
    }

    // Switch namespace
    systemd_nsenter(systemd_pid, cwd);

    // TODO:  Login the user

    err(EXIT_FAILURE, "Entering systemd namespace failed!");
}