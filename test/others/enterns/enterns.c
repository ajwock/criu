/*
 * A test program that demonstrates using criu's --enter_mntns and --enter_pidns
 * options.
 *
 * 
 */

#include "common.h"

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];

struct sockaddr_un addr = {
    AF_UNIX,
    "./enterns.sock"
};
socklen_t addrsize;

struct child_args {
    int argc;
    char **argv;
};

void stun(void) {
    sleep(1000);
}

void inner_child(struct child_args *cargs) {
    execl("checkpointee",
        "checkpointee",
        NULL);
    fprintf(stderr, "Failed to exec: %s\n", strerror(errno));
    exit(1);
}

#define PROCESS_DRIVEUP 1100

void spawn_child(struct child_args *cargs) {
    pid_t child;
    int status;
    child = fork();
    if (child < 0) {
        fprintf(stderr, "Failed to fork, child of init\n");
        exit(1);
    } else if (child == 0) {
        inner_child(cargs);
    }

    checked_waitpid(child, &status, 0);
}

int init(void *args)
{
    pid_t driveup[PROCESS_DRIVEUP];
    int rc, status;
    struct child_args *cargs = (struct child_args *) args;

    if (mount(NULL, "/proc", NULL, MS_PRIVATE, NULL) == -1) {
        fprintf(stderr, "Failed to set proc propagation to private\n");
        exit(1);
    }

    if (mount(NULL, "/proc", "proc", 0, NULL) == -1) {
        fprintf(stderr, "Failed to remount /proc in child namespace: %s\n",
            strerror(errno));
        exit(1);
    }

    for (int i = 0; i < PROCESS_DRIVEUP; i++) {
        driveup[i] = fork();
        if (driveup[i] < 0) {
            fprintf(stderr, "Failed to fork, driveup process\n");
            exit(1);
        } else if (driveup[i] == 0) {
            // We're simply driving up the pid number in the namespace
            // so that upon restore we don't collide with a low-pid process.
            // At least hopefully we won't.
            exit(0);
        }
    }
    for (int i = 0; i < PROCESS_DRIVEUP; i++) {
        rc = kill(driveup[i], 9);
        if (rc == -1) {
            fprintf(stderr, "Failed to kill driveup process\n");
            exit(1);
        }
        rc = waitpid(driveup[i], &status, 0);
        if (rc != driveup[i]) {
            fprintf(stderr, "Waitpid failed, driveup process\n");
            exit(1);
        }
    }

    spawn_child(cargs);
    printf("init exited\n");
    exit(0);
}

#define CRIU_BINARY "../../../criu/criu"

int main(int argc, char **argv)
{
    int pid, criu_pid, status, rc, fd;
    int sock, child_fd;
    char lfconts;
    char sync = 'Z';
    char pidbuf[32];
    struct child_args cargs = {argc, argv};
    struct ucred ids;
    socklen_t ids_len = sizeof(ids);
    addrsize = sizeof(addr.sun_family) + strlen(addr.sun_path);
    log_file = stderr;

    sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        fprintf(stderr, "Failed to create socket\n");
        exit(1);
    }

    rc = bind(sock, &addr, addrsize); 
    if (rc == -1) {
        fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
        exit(1);
    }

    pid = clone(&init, child_stack + STACK_SIZE,
        CLONE_NEWNS | CLONE_NEWPID , &cargs);
    if (pid < 0) {
        fprintf(stderr, "Failed to fork\n");
        exit(1);
    }

    rc = listen(sock, 1);
    if (rc == -1) {
        fprintf(stderr, "Failed to listen to socket: %s\n", strerror(errno));
        exit(1);
    }

    child_fd = accept(sock, &addr, &addrsize);
    if (rc == -1) {
        fprintf(stderr, "Failed to accept connection on socket: %s\n", strerror(errno));
        exit(1); 
    }

    if (getsockopt(child_fd, SOL_SOCKET, SO_PEERCRED, &ids, &ids_len) == -1) {
        fprintf(stderr, "Failed to get peercreds from socket: %s\n", strerror(errno));
        exit(1);
    }

    if (send(child_fd, &sync, sizeof(sync), 0) != sizeof(sync)) {
        fprintf(stderr, "Failed to send sync message: %s\n", strerror(errno));
        exit(1);
    }
    close(sock);

    sprintf(pidbuf, "%d", ids.pid);

    //stun();
    mkdir("./checkpoint", 0777);
    printf("Checkpointing...\n");
    criu_pid = fork();
    if (criu_pid < 0) {
        fprintf(stderr, "Failed to fork for criu");
        exit(1);
    } else if (criu_pid == 0) {
        execl(CRIU_BINARY,
            CRIU_BINARY,
            "dump",
            "--enter-mntns",
            "--enter-pidns",
            "--images-dir",
            "./checkpoint",
            "-t",
            pidbuf,
            NULL);
        fprintf(stderr, "Execl failed: %s\n", strerror(errno));

        exit(1);
    }

    checked_waitpid(criu_pid, &status, 0);
    printf("Checkpoint successful.\n");
    lfconts = '1';
    fd = checked_open("./enterns.lockfile", O_WRONLY);
    lock_file(fd);
    checked_write(fd, &lfconts, sizeof(char)); 
    unlock_file(fd);
    close(fd);

    printf("Restoring...\n");
    criu_pid = fork();
    if (criu_pid < 0) {
        fprintf(stderr, "Failed to fork for criu");
        exit(1);
    } else if (criu_pid == 0) {
        execl(CRIU_BINARY,
            CRIU_BINARY,
            "--images-dir",
            "./checkpoint",
            "restore",
            NULL);
        fprintf(stderr, "Execl failed\n");
        exit(1);
    }

    if (waitpid(criu_pid, &status, 0) != criu_pid) {
        fprintf(stderr, "Waitpid on restored process failed\n");
        exit(1);
    }

    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "%s:%d: Got bad exit code %d from restored process.\n"
            "child log_file: ./checkpointee.log\n", __FILE__, __LINE__,
            WEXITSTATUS(status));
        exit(1);
    }
    printf("Restore successful\nPASS\n");
    return 0;
}
