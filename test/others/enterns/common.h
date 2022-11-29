#define _GNU_SOURCE 1
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/futex.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

FILE *log_file;

int checked_open_f(char *fname, int flags, char *file, int line) {
    int fd = open(fname, flags);
    if (fd == -1) {
        fprintf(log_file, "%s:%d: Failed to open: %s\n", file, line, strerror(errno));
        exit(1);
    }
    return fd;
}

#define checked_open(fn, f)  checked_open_f(fn, f, __FILE__, __LINE__)

void checked_write_f(int fd, void *buf, size_t size, char *file, int line) {
    if (write(fd, buf, size) != size) {
        fprintf(log_file, "%s:%d: Failed to write\n", file, line);
        exit(1);
    }
}

#define checked_write(fd, buf, size) checked_write_f(fd, buf, size, __FILE__, __LINE__)

void checked_waitpid_f(pid_t p, int *status, int flags, char *file, int line) {
    if (waitpid(p, status, flags) != p) {
        fprintf(log_file, "%s:%d: Waitpid failed\n", file, line);
        exit(1);
    }
    if (WEXITSTATUS(*status) != 0) {
        fprintf(log_file, "%s:%d: Got bad exit code from child\n", file, line);
        exit(1);
    }
}

#define checked_waitpid(p, status, flags) checked_waitpid_f(p, status, flags, __FILE__, __LINE__)

void checked_read_f(int fd, void *buf, size_t size, char *file, int line) {
    if (read(fd, buf, size) != size) {
        fprintf(log_file, "%s:%d: Failed to read\n", file, line);
        exit(1);
    }
}

#define checked_read(fd, buf, size) checked_read_f(fd, buf, size, __FILE__, __LINE__)

void lock_file(int fd) {
    if (flock(fd, LOCK_EX) == -1) {
        fprintf(log_file, "Failed to lock file\n");
        exit(1);
    }
}

void unlock_file(int fd) {
    if (flock(fd, LOCK_UN) == -1) {
        fprintf(log_file, "Failed to lock file\n");
        exit(1);
    }
}
