#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Open the syslog
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    // Check if both arguments are provided
    if (argc != 3) {
        syslog(LOG_ERR, "Error: Both arguments must be specified");
        closelog();
        exit(EXIT_FAILURE);
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    // Open the file for writing
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error: Could not open file %s: %s", writefile, strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    // Write the string to the file
    size_t len = strlen(writestr);
    if (fwrite(writestr, sizeof(char), len, file) < len) {
        syslog(LOG_ERR, "Error: Could not write to file %s: %s", writefile, strerror(errno));
        fclose(file);
        closelog();
        exit(EXIT_FAILURE);
    }

    // Log the debug message
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    // Close the file
    fclose(file);

    // Close the syslog
    closelog();

    return EXIT_SUCCESS;
}
