#define _GNU_SOURCE

#include <string.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <libgen.h>
#include <publib.h>

#define EX_EXEC_FAILED		126	/* Program located, but not usable. */
#define EX_EXEC_ENOENT		127	/* Could not find program to exec.  */
#define errexec(name)	err(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED, \
			"failed to execute %s", name)


#define DEFAULT_SHELL "/bin/sh"

void __attribute__((__noreturn__)) exec_shell(void)
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


static void continue_as_child(void)
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

void open_and_setns(pid_t pid, char* type, int nstype) {
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

void systemd_nsenter(pid_t systemd_pid, char* wd_path)
{
    int wd_fd;

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

    exec_shell();
}