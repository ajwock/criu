#include <termios.h>

#include "common.h"

struct sockaddr_un addr = {
    AF_UNIX,
    "./enterns.sock"
};
socklen_t addrsize;

void close_log(void) {
    fflush(log_file);
    fclose(log_file);
}

int main(int argc, char **argv) {
    int fd, sockfd, rc;
    pid_t sid;
    FILE *newlog;
    char lfconts = '0';
    char sync = 0;
    // 20ms
    struct timespec slp = { 0, 20000000 };
    log_file = stderr;

    printf("In child\n");


    sid = setsid();
    if (sid == -1) {
        fprintf(stderr, "Failed to set sid for test: %s\n", strerror(errno));
     //   exit(1);
    }

    fd = checked_open("./enterns.lockfile", O_WRONLY | O_CREAT);
    checked_write(fd, &lfconts, sizeof(lfconts));
    close(fd);

    addrsize = sizeof(addr.sun_family) + strlen(addr.sun_path);
    
    sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Couldn't make socket in child: %s\n", strerror(errno));
        exit(1);
    }

    rc = connect(sockfd, (const struct sockaddr *) &addr, sizeof(addr.sun_family) + strlen(addr.sun_path));
    if (rc == -1) {
        fprintf(stderr, "Couldn't connect to socket in child: %s\n", strerror(errno));
        exit(1);
    }

    newlog = fopen("./checkpointee.log", "w");
    if (newlog == NULL) {
        fprintf(stderr, "Failed to open log_file in child: %s\n", strerror(errno));
        exit(1);
    }
    fprintf(newlog, "hello\n");
    atexit(&close_log);

    printf("Receiving...\n");
    rc = recv(sockfd, &sync, sizeof(sync), 0);
    if (rc != sizeof(sync)) {
        fprintf(stderr, "Child failed to receive sync msg: %s\n", strerror(errno));
        exit(1);
    }
    printf("Received: %c\n", sync);

    if (sync != 'Z') {
        fprintf(stderr, "Child didn't get proper sync msg: %s\n", strerror(errno));
    }
    // Must close output channels for checkpoint to work across mount namespace
    fclose(stdin);
    log_file = newlog;
    fclose(stderr);
    fclose(stdout);
    close(sockfd);

    while (lfconts == '0') {
        nanosleep(&slp, NULL);
        fd = checked_open("./enterns.lockfile", O_RDONLY);
        lock_file(fd);
        checked_read(fd, &lfconts, sizeof(char));
        unlock_file(fd);
        close(fd);
    }




    fprintf(log_file, "Pass\n");
    exit(0);
}
