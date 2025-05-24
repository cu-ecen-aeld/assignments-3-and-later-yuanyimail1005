#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    if (0 == pthread_mutex_lock(&thread_func_args->mutex)) {
        sleep(thread_func_args->wait_time);
        pthread_mutex_unlock(&thread_func_args->mutex);
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    pthread_t thread;
    thread_data *data = malloc(sizeof(thread_data)); // Allocate memory for thread_data

    if (!data) {
        fprintf(stderr, "Memory allocation failed!\n");
        return false;
    }

    data->wait_time = 3;

    pthread_mutex_init(&data->mutex, NULL);

    if (pthread_create(&thread, NULL, threadfunc, data) != 0) {
        fprintf(stderr, "Thread creation failed!\n");
        free(data);
        pthread_mutex_destroy(&data->mutex);
        return false;
    }

    pthread_join(thread, NULL);

    pthread_mutex_destroy(&data->mutex);

    return true;
}

