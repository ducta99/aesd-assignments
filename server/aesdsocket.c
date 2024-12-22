#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "../examples/threading/threading.h"

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd = -1;
int client_fd = -1;
FILE *file = NULL;
int daemon_mode = 0;
bool exit_flag = false;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
// bool exit_flag = false;
thread_data_t *head = NULL;
pthread_t timer_thread;

void handle_signal(int sig) {
    static int already_cleaned_up = 0;

    if (already_cleaned_up) {
        return;  // If already cleaned up, return immediately
    }
    already_cleaned_up = 1;  // Mark as cleaned up

    syslog(LOG_INFO, "Caught signal %d, exiting", sig);
    
    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;  // Ensure the descriptor is not reused
    }
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;  // Ensure the descriptor is not reused
    }
    if (file) {
        if (fclose(file) != 0) {
            syslog(LOG_ERR, "Failed to close file: %s", strerror(errno));
        }
        file = NULL;  // Ensure the pointer is not reused
    }
    if (remove(FILE_PATH) != 0) {
        syslog(LOG_ERR, "Failed to remove file %s: %s", FILE_PATH, strerror(errno));
    } else {
        syslog(LOG_INFO, "Successfully removed file %s", FILE_PATH);
    }
    // Cleanup threads
    pthread_mutex_lock(&file_mutex);
    thread_data_t *current = head;
    while (current) {
        pthread_join(current->thread_id, NULL);
        close(current->client_fd);
        thread_data_t *temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_unlock(&file_mutex);
    exit_flag = true;
    pthread_join(timer_thread, NULL);
    closelog();
    exit(0);
}


void setup_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork process: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }
    
    // Child process continues
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new session: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Redirect standard files to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

// Function to periodically write timestamps
void *timestamp_thread(void *arg) {
    while (!exit_flag) {
        sleep(10);

        // Get the current time
        time_t now = time(NULL);
        struct tm *time_info = localtime(&now);
        char timestamp[BUFFER_SIZE];

        strftime(timestamp, BUFFER_SIZE, "timestamp:%a, %d %b %Y %H:%M:%S %z\n", time_info);

        // Write the timestamp to the file
        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen(FILE_PATH, "a");
        if (file) {
            fputs(timestamp, file);
            fflush(file);
            fclose(file);
        } else {
            syslog(LOG_ERR, "Failed to open file for timestamp: %s", strerror(errno));
        }
        pthread_mutex_unlock(&file_mutex);
    }

    // pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int opt;

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Process command line arguments
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    setup_signal_handler();

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    // Set SO_REUSEADDR to allow reuse of the port
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        syslog(LOG_ERR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Bind to port 9000
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Daemonize if the -d flag is set
    if (daemon_mode) {
        daemonize();
    }

    // Listen for incoming connections
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Start the timestamp thread
    if (pthread_create(&timer_thread, NULL, timestamp_thread, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timestamp thread: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }
        
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Allocate and initialize thread data
        thread_data_t *thread_data = malloc(sizeof(thread_data_t));
        if (!thread_data) {
            syslog(LOG_ERR, "Memory allocation failed");
            close(client_fd);
            continue;
        }

        start_thread_obtaining_mutex(thread_data,&file_mutex,client_fd);

        // Add thread data to the linked list
        pthread_mutex_lock(&file_mutex);
        thread_data->next = head;
        head = thread_data;
        pthread_mutex_unlock(&file_mutex);

        // Log and close connection
        // syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

    }

    if (file) {
        fclose(file);
        file = NULL;
    }
    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;  // Ensure the descriptor is not reused
    }

    return 0;
}
