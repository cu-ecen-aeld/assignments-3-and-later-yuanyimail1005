#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    openlog(NULL,0,LOG_USER);

    // Validate arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Error: Invalid arguments\nUsage: %s <file_path> <text_to_write>\n", argv[0]);
        return 1;
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    syslog(LOG_DEBUG, "Writing %s to file %s", writestr, writefile);

    // Create necessary directories
    char dirpath[1024];
    strncpy(dirpath, writefile, sizeof(dirpath));
    char *lastSlash = strrchr(dirpath, '/');
    if (lastSlash != NULL) {
        *lastSlash = '\0'; // Remove filename to get directory path
        if (mkdir(dirpath, 0755) && errno != EEXIST) {
            syslog(LOG_ERR, "Error: Could not create directory %s\n", dirpath);
            return 1;
        }
    }

    // Open file for writing
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error: Could not create file %s\n", writefile);
        return 1;
    }

    // Write string to file
    fprintf(file, "%s\n", writestr);
    fclose(file);

    return 0;
}
