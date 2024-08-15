#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
// #define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // Cast the parameter to the thread_data structure
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // Wait for a specified time before obtaining the mutex
    sleep(thread_func_args->wait_time_before);

    // Lock the mutex
    if (pthread_mutex_lock(thread_func_args->mutex) == 0) {
        DEBUG_LOG("Mutex locked by thread");

        // Wait for a specified time after obtaining the mutex
        sleep(thread_func_args->wait_time_after);

        // Unlock the mutex
        pthread_mutex_unlock(thread_func_args->mutex);
        DEBUG_LOG("Mutex unlocked by thread");

        // Mark the operation as successful
        thread_func_args->thread_complete_success = true;
    } else {
        // Mutex locking failed
        thread_func_args->thread_complete_success = false;
    }

    // Return the thread data so it can be freed by the joiner thread
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
    // Allocate memory for thread_data
    struct thread_data* data = (struct thread_data*)malloc(sizeof(struct thread_data));
    if (data == NULL) {
        // Memory allocation failed
        return false;
    }

    // Setup mutex and wait arguments
    data->mutex = mutex;
    data->wait_time_before = wait_to_obtain_ms / 1000;  // Convert milliseconds to seconds
    data->wait_time_after = wait_to_release_ms / 1000;  // Convert milliseconds to seconds
    data->thread_complete_success = false;  // Initialize as false

    // Create the thread, passing the thread_data to the thread function
    int result = pthread_create(thread, NULL, threadfunc, data);
    if (result != 0) {
        // Thread creation failed, free allocated memory
        free(data);
        return false;
    }

    // Return true if successful
    return true;
}

