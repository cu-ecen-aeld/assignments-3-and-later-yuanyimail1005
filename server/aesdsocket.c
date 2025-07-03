#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <pthread.h>
#if USE_AESD_CHAR_DEVICE == 1
	#define DATA_PATH ("/dev/aesdchar")
	#include "../aesd-char-driver/aesd_ioctl.h"
	#define AESD_IOCTLSEEKTOCMD "AESDCHAR_IOCSEEKTO:"
#else
	#define DATA_PATH ("var/tmp/aesdsocketdata")
#endif

#define BUF_SIZE 1024

int sockfd;
int newfd;
int fd;

pthread_mutex_t s_mutex;

struct thread_data {
	pthread_t thread_id;
	int socket_id;
	bool isDone;
	SLIST_ENTRY(thread_data) entries;
};

SLIST_HEAD(slisthead, thread_data) head;

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void terminate(int signum)
{
	syslog(LOG_INFO, "Caught signal, exiting");
	closelog();
	close(newfd);
	close(sockfd);
	close(fd);

	#if USE_AESD_CHAR_DEVICE == 0
		remove("/var/tmp/aesdsocketdata");
	#endif

	exit(0);
}

void* socket_handler(void* args){
    struct thread_data *t_data = (struct thread_data *)args;
    int connection = t_data->socket_id;
    char local_buffer[BUF_SIZE];

    int size = recv(connection, local_buffer, sizeof(local_buffer)-1, 0);
    local_buffer[size] = '\0';
    
    pthread_mutex_lock(&s_mutex);
#ifdef USE_AESD_CHAR_DEVICE
    int fp = open(DATA_PATH, O_RDWR);
#else
    int fp = open(DATA_PATH, O_RDWR|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG|S_IRWXO);
#endif

    while (size > 0) {
            char *newline_pos = strchr(local_buffer, '\n');
            if (newline_pos) {
                    size_t index = newline_pos - local_buffer;
                    local_buffer[index+1] = '\0';
            }
            unsigned char len = strlen(local_buffer);
#ifdef USE_AESD_CHAR_DEVICE
            if (strstr(local_buffer, "AESDCHAR_IOCSEEKTO"))
            {
                    int write_cmd, write_cmd_offset;
                    sscanf(local_buffer, "AESDCHAR_IOCSEEKTO:%d,%d", &write_cmd, &write_cmd_offset);
                    struct aesd_seekto seekto;
                    seekto.write_cmd = write_cmd;
                    seekto.write_cmd_offset = write_cmd_offset;
                    
		    if (ioctl(fp, AESDCHAR_IOCSEEKTO, &seekto) == -1) {
                            perror("ioctl failed");
                    }
            }
            else{
                    write(fp, local_buffer, len);
            }
#else
            write(fp, local_buffer, len);
#endif
            if (newline_pos)
                    break;
            memset(local_buffer, 0, sizeof(local_buffer));
            size = recv(connection, local_buffer, sizeof(local_buffer)-1, 0);
            local_buffer[size] = '\0';
    }

    ssize_t sent = read(fp, local_buffer, sizeof(local_buffer)-1);
    local_buffer[sent] = '\0';
    while (sent > 0) {
            send(connection, local_buffer, strlen(local_buffer), 0);
            memset(local_buffer, 0, sizeof(local_buffer));
            sent = read(fp, local_buffer, sizeof(local_buffer)-1);
            local_buffer[sent] = '\0';
    }
    pthread_mutex_unlock(&s_mutex);
    memset(local_buffer, 0, sizeof(local_buffer));
    close(fp);
    close(connection);
    pthread_exit(NULL);
    return NULL;
}

void* timestamp_handler(void* arg) {
	for(;;) {
		sleep(10);
		pthread_mutex_lock(&s_mutex);
		if ((fd = open(DATA_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644)) >=0) {
			time_t t_now = time(NULL);
			struct tm *tm_data = localtime(&t_now);
			char buffer_t[100];
			strftime(buffer_t, sizeof(buffer_t), "timestamp:&a, %d %b %Y %H:%M:%S %z\n", tm_data);
			write(fd, buffer_t, strlen(buffer_t));
			close(fd);
		}
		pthread_mutex_unlock(&s_mutex);
	}
	return NULL;
}

int main(int argc, char *argv[]){
	char s[INET6_ADDRSTRLEN];
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	bool daemonize = false;
	int opt;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE;
	
	pthread_mutex_init(&s_mutex, NULL);
	
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
			case 'd':
				daemonize = true;
				break;
			default:
				//usage error
				return -1;
		}
	}

	openlog (NULL, 0, LOG_USER);

	if ((getaddrinfo(NULL, "9000", &hints, &servinfo)) == -1) {
		//error
		return -1;
	}
	if((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
		//error
		return -1;
	}
	int sockopt = 1;
	if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt))) == -1) {
			//error
			return -1;
	}
	if ((bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
		//error
		return -1;
	}

	freeaddrinfo(servinfo);

	if(daemonize) {
		pid_t pid;
		pid = fork();
		if (pid == -1){
			//error
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			exit(EXIT_SUCCESS);
		}
		if (setsid() == -1) {
			//error
			exit(EXIT_FAILURE);
		}
		if (chdir("/") == -1) {
			//error
			exit(EXIT_FAILURE);
		}
	}

	if ((listen(sockfd, 10)) == -1) {
		//error
		close(sockfd);
		return -1;
	}
	
	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	
	#if USE_AESD_CHAR_DEVICE == 0
		pthread_t timestamp_thread;
		pthread_create(&timestamp_thread, NULL, timestamp_handler, NULL);
	#endif

	printf("\n test1");
	for(;;) {
		if ((newfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1) {
			//error
			return -1;
		}
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		syslog (LOG_INFO, "Accepted Connection from %s", s);

		struct thread_data *t_data = malloc(sizeof(struct thread_data));
		t_data->socket_id = newfd;
		SLIST_INSERT_HEAD(&head, t_data, entries);
		pthread_create(&t_data->thread_id, NULL, socket_handler, t_data);
	}

	struct thread_data *t_data;
	while(!SLIST_EMPTY(&head)) {
		t_data = SLIST_FIRST(&head);
		pthread_join(t_data->thread_id, NULL);
		SLIST_REMOVE_HEAD(&head, entries);
		free(t_data);
	}
	close(sockfd);
}
