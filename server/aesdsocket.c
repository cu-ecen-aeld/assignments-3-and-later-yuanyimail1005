#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>

#define PORT 9000
#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int server_fd;
int client_fd;
FILE *file = NULL;

void cleanup_and_exit(int signum) {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (client_fd) close(client_fd);
    if (server_fd) close(server_fd);
    if (file) fclose(file);
    file = NULL;
    remove(FILEPATH);
    closelog();
    exit(0);
}

void start_server(int daemon_mode) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int opt = 1;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind socket to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Binding failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Daemon fork failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            exit(0);
        }
    }

    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        syslog(LOG_ERR, "Listening failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Connection acceptance failed");
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        file = fopen(FILEPATH, "a+");
        if (!file) {
            syslog(LOG_ERR, "File operation failed");
            close(client_fd);
            continue;
        }

        ssize_t received;
        while ((received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, received, file);
            fflush(file);

            if (strchr(buffer, '\n')) {
                fseek(file, 0, SEEK_SET);
                while (fgets(buffer, BUFFER_SIZE, file)) {
                    send(client_fd, buffer, strlen(buffer), 0);
                }
                break;
            }
        }

        fclose(file);
        file = NULL;
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
        close(client_fd);
    }
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    int daemon_mode = (argc > 1 && strcmp(argv[1], "-d") == 0);
    start_server(daemon_mode);

    return 0;
}

