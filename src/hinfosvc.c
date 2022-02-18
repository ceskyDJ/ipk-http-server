#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/**
 * Maximum length of hostname (fully qualified)
 *
 * It is defined by UNIX standard (see source)
 *
 * Source: https://man7.org/linux/man-pages/man7/hostname.7.html
 */
#define HOSTNAME_LENGTH 253
/**
 * Maximum length of cpu info string
 *
 * This value is just an estimation based on values seen on local
 * computer and school servers + some reserve
 */
#define CPU_INFO_LENGTH 100

/**
 * Skips a line (or the rest of it) in the file
 *
 * @param file Opened file where to do the operation
 * @pre file != NULL
 * @pre file is valid opened file
 */
void skip_line(FILE *file) {
    int c;

    while ((c = fgetc(file)) != EOF && c != '\n') {
        // This isn't an accident, we just read characters until end of the line or file is reached
    }
}

/**
 * Finds and returns hostname of the computer running this program
 *
 * @param hostname Pointer to place where to save found hostname to
 * @return 0 => success, 1 => failure
 * @pre hostname != NULL
 */
int get_hostname(char *hostname) {
    FILE *hostname_file;

    // Get output of `hostname` command
    hostname_file = popen("/bin/hostname -f", "r");
    if (hostname_file == NULL) {
        fprintf(stderr, "Cannot execute command `/bin/hostname -f`");
        return 1;
    }

    // Just read the needed value
    fgets(hostname, HOSTNAME_LENGTH + 1, hostname_file);

    // Remove '\n' from the end of the value
    hostname[strlen(hostname) - 1] = '\0';

    pclose(hostname_file);
    return 0;
}

/**
 * Finds and returns computer's CPU info
 *
 * @param cpu_info Pointer to place where to save found cpu info
 * @return 0 => success, 1 => failure
 * @pre cpu_info != NULL
 */
int get_cpu_info(char *cpu_info) {
    FILE *proc_cpu_info;
    bool found = false;
    char buffer[11]; // strlen("model name") = 10
    int c;

    proc_cpu_info = fopen("/proc/cpuinfo", "r");
    if (proc_cpu_info == NULL) {
        return 1;
    }

    // Try to find row that starts with "model name"
    while (found == false) {
        if (fgets(buffer, sizeof(buffer), proc_cpu_info) == NULL) {
            // The end of the file and a wanted row not found --> we can't get needed CPU info
            fclose(proc_cpu_info);
            return 1;
        }

        if (strcmp(buffer, "model name") != 0) {
            // Not a wanted row --> skip it
            skip_line(proc_cpu_info);
        } else {
            found = true;
        }
    }

    // We are on the line with needed data, it looks like:
    // model name      : Intel(R) Xeon(R) CPU E5-2620 v3 @ 2.40GHz
    // So, we need to skip whitespace chars and ':'
    while ((c = fgetc(proc_cpu_info)) != EOF && (isspace(c) || c == ':')) {
        // This while is just for skipping some part of the file, so it doesn't need any commands in
    }

    // Skipping will skip the first char of the needed value, so it must be reverted
    ungetc(c, proc_cpu_info);

    // Just read needed value
    fgets(cpu_info, CPU_INFO_LENGTH, proc_cpu_info);

    // Remove '\n' from the end of the value
    cpu_info[strlen(cpu_info) - 1] = '\0';

    fclose(proc_cpu_info);
    return 0;
}

/**
 * Init (main) function of the program
 *
 * @param argc Number of CLI arguments
 * @param argv CLI arguments as array of "strings"
 * @return Program's exit code
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    char hostname[HOSTNAME_LENGTH + 1] = "";
    char cpu_info[CPU_INFO_LENGTH + 1] = "";

    if (get_hostname(hostname) != 0) {
        return 1;
    }
    if (get_cpu_info(cpu_info) != 0) {
        return 1;
    }

    printf("Hostname: %s\n", hostname);
    printf("CPU info: %s\n", cpu_info);

    return 0;
}
