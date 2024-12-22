#include "threading.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/socket.h>
// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

// void* threadfunc(void* thread_param)
// {

//     // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
//     // hint: use a cast like the one below to obtain thread arguments from your parameter
//     //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
//     return thread_param;
// }


// bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
// {
//     /**
//      * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
//      * using threadfunc() as entry point.
//      *
//      * return true if successful.
//      *
//      * See implementation details in threading.h file comment block
//      */
//     return false;
// }

#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

// struct thread_data {
//     int client_fd;
//     pthread_mutex_t *file_mutex;
// };

// Function to handle client connection
void *handle_client(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int client_fd = data->client_fd;
    pthread_mutex_t *file_mutex = data->file_mutex;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';

        // Synchronize file writes
        pthread_mutex_lock(file_mutex);
        FILE *file = fopen(FILE_PATH, "a");
        if (file) {
            fputs(buffer, file);
            fflush(file);
            fclose(file);
        } else {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        }
        pthread_mutex_unlock(file_mutex);

        if (strchr(buffer, '\n')) {
            // Read and send back the file content
            pthread_mutex_lock(file_mutex);
            file = fopen(FILE_PATH, "r");
            if (file) {
                char file_buffer[BUFFER_SIZE];
                while (fgets(file_buffer, BUFFER_SIZE, file)) {
                    send(client_fd, file_buffer, strlen(file_buffer), 0);
                }
                fclose(file);
            } else {
                syslog(LOG_ERR, "Failed to open file for reading: %s", strerror(errno));
            }
            pthread_mutex_unlock(file_mutex);
        }
    }

    close(client_fd);
    syslog(LOG_INFO, "Closed connection for client_fd %d", client_fd);

    pthread_exit(NULL);
}


bool start_thread_obtaining_mutex(thread_data_t *thread_data, pthread_mutex_t *file_mutex, int client_fd) {
    // Allocate and initialize thread data
    if (!thread_data) {
        syslog(LOG_ERR, "Memory allocation failed");
        close(client_fd);
        return false;
    }

    thread_data->client_fd = client_fd;
    thread_data->file_mutex = file_mutex;
    thread_data->next = NULL;

    // Create a thread for the new connection
    if (pthread_create(&thread_data->thread_id, NULL, handle_client, thread_data) != 0) {
        syslog(LOG_ERR, "Failed to create thread: %s", strerror(errno));
        close(client_fd);
        free(thread_data);
        return false;
    }

    return true; 
}
