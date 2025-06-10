#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>

#define PORT 9000
#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

pthread_mutex_t file_mutex;
typedef struct thread_info {
    pthread_t thread;
    int client_fd;
    struct thread_info *next;
} thread_info_t;

thread_info_t *head = NULL;
int server_fd;
pthread_t timestamp_thread;

void *handle_client(void *arg) {
    thread_info_t *info = (thread_info_t *)arg;
    char buffer[BUFFER_SIZE];
    FILE *file = NULL;
    ssize_t received;

    file = fopen(FILEPATH, "a+");
    if (!file) {
        syslog(LOG_ERR, "File operation failed");
        close(info->client_fd);
        return NULL;
    }

	while ((received = recv(info->client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        pthread_mutex_lock(&file_mutex);
        fwrite(buffer, 1, received, file);
		fflush(file);

		if (strchr(buffer, '\n')) {
			fseek(file, 0, SEEK_SET);
			while (fgets(buffer, BUFFER_SIZE, file)) {
				send(info->client_fd, buffer, strlen(buffer), 0);
			}
            pthread_mutex_unlock(&file_mutex);
			break;
		}
        pthread_mutex_unlock(&file_mutex);
	}

	fclose(file);
	close(info->client_fd);
    return NULL;
}

void sig_handler(int sig) {
    if (server_fd) close(server_fd);
    while (head) {
        pthread_join(head->thread, NULL);
        thread_info_t *temp = head;
        head = head->next;
        free(temp);
    }
    pthread_cancel(timestamp_thread);
    pthread_mutex_destroy(&file_mutex);
    remove(FILEPATH);
    closelog();
    exit(0);
}

void *append_timestamp(void *arg) {
    while (1) {
        sleep(10); // Wait 10 seconds

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[128];
        strftime(timestamp, sizeof(timestamp), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", tm_info);

        pthread_mutex_lock(&file_mutex);
        int fd = open(FILEPATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
        write(fd, timestamp, strlen(timestamp));
        close(fd);
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

void start_server(int daemon_mode) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    pthread_mutex_init(&file_mutex, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    pthread_create(&timestamp_thread, NULL, append_timestamp, NULL);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Connection acceptance failed");
            continue;
        }
        thread_info_t *new_thread = malloc(sizeof(thread_info_t));
        new_thread->client_fd = client_fd;
        new_thread->next = head;
        head = new_thread;

        pthread_create(&new_thread->thread, NULL, handle_client, new_thread);
    }

    pthread_join(timestamp_thread, NULL);
    return;
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    int daemon_mode = (argc > 1 && strcmp(argv[1], "-d") == 0);
    start_server(daemon_mode);

    return 0;
}
