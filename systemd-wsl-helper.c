/* See LICENSE for license details. */

#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <publib.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <sys/wait.h>

#include <proc/readproc.h>
#include <security/pam_appl.h>

#define EX_EXEC_FAILED		126	/* Program located, but not usable. */
#define EX_EXEC_ENOENT		127	/* Could not find program to exec.  */
#define errexec(name)	err(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED, \
			"failed to execute %s", name)
#define DEFAULT_SHELL "/bin/sh"

static pid_t get_systemd_pid(void);
static void login_user(char *username);
static void __attribute__((__noreturn__)) exec_shell(void);
static void continue_as_child(void);
static void open_and_setns(pid_t pid, char* type, int nstype);
static void systemd_nsenter(pid_t systemd_pid, char* wd_path);

/**
 * @brief Get the systemd pid object
 * 
 * @return pid_t 
 */
static pid_t
get_systemd_pid(void)
{
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

/**
 * @brief 
 * 
 * @param username 
 */
static void
login_user(char* username)
{
    int rc;
    // TODO:  verify that service name is correct
    char service_name[] = "login";
    pam_handle_t *pamh = NULL;

    rc = pam_start(service_name, username, NULL, &pamh);

}

static void __attribute__((__noreturn__))
exec_shell(void)
{
	const char *shell = getenv("SHELL");
	char *shellc;
	const char *shell_basename;
	char *arg0;

	if (!shell) {
		shell = DEFAULT_SHELL;
    }

	if ((shellc = strdup(shell)) == NULL) {
        perror("strdup");
        err(EXIT_FAILURE, "insufficient memory");
    }
	
    shell_basename = basename(shellc);

    if ((arg0 = malloc(strlen(shell_basename) + 2)) == NULL) {
        perror("malloc");
        err(EXIT_FAILURE, "insufficient memory");
    }

	arg0[0] = '-';
	strcpy(arg0 + 1, shell_basename);

	execl(shell, arg0, (char *)NULL);
	errexec(shell);
}


static void
continue_as_child(void)
{
    pid_t child = fork();
    int status;
    pid_t ret;

    if (child < 0) {
        err(EXIT_FAILURE, "fork failed");
    }

    /* Only the child returns */
    if (child == 0) {
        return;
    }

    for (;;) {
        ret = waitpid(child, &status, WUNTRACED);
        if ((ret == child) && (WIFSTOPPED(status))) {
            /* The child suspended so suspend us as well */
            kill(getpid(), SIGSTOP);
            kill(child, SIGCONT);
        } else {
            break;
        }
    }

    /* Return the child's exit code if possible */
    if (WIFEXITED(status)) {
        exit(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        kill(getpid(), WTERMSIG(status));
    }

    exit(EXIT_FAILURE);
}

static void
open_and_setns(pid_t pid, char* type, int nstype)
{
    char ns_path[PATH_MAX];
    int ns_fd;
    
    snprintf(ns_path, sizeof(ns_path), "/proc/%i/ns/%s", pid, type);
    
    if ((ns_fd = open(ns_path, O_RDONLY)) < 0) {
        err(EXIT_FAILURE, "unable to open namespace %s", ns_path);
    }

    if (setns(ns_fd, nstype) < 0) {
        err(EXIT_FAILURE, "unable to reassociate to namespace %s", ns_path);
    }

    close(ns_fd);
}

void
systemd_nsenter(pid_t systemd_pid, char* wd_path)
{
    int wd_fd;
    uid_t uid = getuid();
    uid_t gid = getgid();

    // TODO:  This could be greatly simplified in kernel 5.8+
    open_and_setns(systemd_pid, "pid", CLONE_NEWPID);
    open_and_setns(systemd_pid, "mnt", CLONE_NEWNS);

    if ((wd_fd = open(wd_path, O_RDONLY)) < 0) {
        err(EXIT_FAILURE, "unable to open directory %s", wd_path);
    }

    if (fchdir(wd_fd) < 0) {
        err(EXIT_FAILURE, "unable to change directory to", wd_path);
    }

    close(wd_fd);

    continue_as_child();

    setuid(uid);
    setgid(gid);

    exec_shell();
}

int main(int argc, char *argv[])
{
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