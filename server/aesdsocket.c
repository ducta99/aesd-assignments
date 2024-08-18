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

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd = -1;
int client_fd = -1;
FILE *file = NULL;
int daemon_mode = 0;

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

    while (1) {
        // Accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }
        
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Open file for appending data
        file = fopen(FILE_PATH, "a+");
        if (!file) {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
            close(client_fd);
            client_fd = -1;  // Ensure the descriptor is not reused
            continue;
        }

        // Receive data and append to file
        while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[bytes_received] = '\0';
            fputs(buffer, file);
            fflush(file);

            if (strchr(buffer, '\n')) {
                // Send file content back to client
                rewind(file);
                char file_buffer[BUFFER_SIZE];
                while (fgets(file_buffer, BUFFER_SIZE, file) != NULL) {
                    send(client_fd, file_buffer, strlen(file_buffer), 0);
                }
            }
        }

        // Log and close connection
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

        if (file) {
            fclose(file);
            file = NULL;
        }
        if (client_fd != -1) {
            close(client_fd);
            client_fd = -1;  // Ensure the descriptor is not reused
        }
    }

    return 0;
}
