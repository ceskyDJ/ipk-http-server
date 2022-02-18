#include <stdio.h>
#include <stdlib.h>

// Source: https://man7.org/linux/man-pages/man7/hostname.7.html
#define HOSTNAME_LENGTH 253

/**
 * Finds and returns hostname of the computer running this program
 *
 * @param hostname Pointer to place where to save found hostname to
 * @pre hostname != NULL
 */
void get_hostname(char *hostname) {
    FILE *hostname_file;

    hostname_file = popen("/bin/hostname -f", "r");
    if (hostname_file == NULL) {
        fprintf(stderr, "Cannot execute command `/bin/hostname -f`");
        exit(1);
    }

    fgets(hostname, HOSTNAME_LENGTH, hostname_file);

    pclose(hostname_file);
}

/**
 * Init (main) function of the program
 *
 * @param argc Number of CLI arguments
 * @param argv CLI arguments as array of "strings"
 * @return Program's exit code
 */
int main(int argc, char *argv[]) {
    char hostname[HOSTNAME_LENGTH + 1] = "";

    get_hostname(hostname);
    printf("Hostname: %s", hostname);

    return 0;
}
