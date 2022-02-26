/**
 * @file system-info.c
 * System information provider
 *
 * @author Michal Šmahel (xsmahe01)
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "system-info.h"

/**
 * Structure of records in /proc/stat
 */
struct proc_stats {
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;
};

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
 * Loads an unsigned long value from the file
 *
 * @param file Opened file to read from
 * @return Loaded unsigned long value
 */
unsigned long load_ul_value(FILE *file) {
    char buffer[21] = "";

    // Load number as string value
    // Max size of unsigned long: 18_446_744_073_709_551_615
    // Source: https://www.tutorialspoint.com/cprogramming/c_data_types.htm
    fscanf(file, "%20s", buffer);

    // Convert to unsigned long
    return strtoul(buffer, NULL, 10);
}

/**
 * Loads CPU statistics from the /proc/stat virtual file
 *
 * @param stats Pointer to the structure proc_stats where to store loaded information
 * @return 0 => success, 1 => error
 */
int load_proc_stats(struct proc_stats *stats) {
    char buffer[4]; // strlen("cpu\0") = 4
    FILE *proc_stats_file;

    // Data are loaded from /proc/stat, that looks like that (the header is implicit):
    //      user    nice   system  idle      iowait irq   softirq  steal  guest  guest_nice
    // cpu  74608   2520   24433   1117073   6176   4054  0        0      0      0
    if ((proc_stats_file = fopen("/proc/stat", "r")) == NULL) {
        fprintf(stderr, "Cannot open file /proc/stat\n");
        return 1;
    }

    // Skip text start of the line ("cpu")
    fgets(buffer, sizeof(buffer), proc_stats_file);
    if (strcmp(buffer, "cpu") != 0) {
        fprintf(stderr, "Bad line read from /proc/stat. The line doesn't start with: cpu\n");
        return 1;
    }

    stats->user = load_ul_value(proc_stats_file); // = user + guest
    stats->nice = load_ul_value(proc_stats_file); // = nice + guest_nice
    stats->system = load_ul_value(proc_stats_file);
    stats->idle = load_ul_value(proc_stats_file);
    stats->iowait = load_ul_value(proc_stats_file);
    stats->irq = load_ul_value(proc_stats_file);
    stats->softirq = load_ul_value(proc_stats_file);
    stats->steal = load_ul_value(proc_stats_file);

    fclose(proc_stats_file);
    return 0;
}

/**
 * Finds and returns hostname of the computer keep_running this program
 *
 * @param hostname Pointer to place where to save found hostname to
 * @return 0 => success, 1 => error
 * @pre hostname != NULL
 */
int get_hostname(char *hostname) {
    FILE *hostname_file;

    // Get output of `hostname` command
    hostname_file = popen("/bin/hostname -f", "r");
    if (hostname_file == NULL) {
        fprintf(stderr, "Cannot execute command `/bin/hostname -f`\n");
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
 * @return 0 => success, 1 => error
 * @pre cpu_info != NULL
 */
int get_cpu_info(char *cpu_info) {
    FILE *proc_cpu_info;
    bool found = false;
    char buffer[11]; // strlen("model name\0") = 11
    int c;

    // CPU information is in the /proc/cpuinfo file --> open it
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
 * Counts CPU load (for all CPU units)
 *
 * @return positive number => CPU load value in %, -1 => error
 *
 * Inspired by: https://stackoverflow.com/a/23376195
 */
int get_cpu_load(void) {
    struct proc_stats prev_st;
    struct proc_stats curr_st;

    unsigned long long prev_idle;
    unsigned long long curr_idle;
    unsigned long long prev_active;
    unsigned long long curr_active;
    unsigned long long prev_total;
    unsigned long long curr_total;
    unsigned long long total_delta;
    unsigned long long idle_delta;

    // First loading of the CPU stats
    if (load_proc_stats(&prev_st) != 0) {
        return -1;
    }

    // Second loading of the CPU stats
    usleep(200 * 1000);
    if (load_proc_stats(&curr_st) != 0) {
        return -1;
    }

    prev_idle = prev_st.idle + prev_st.iowait;
    curr_idle = curr_st.idle + curr_st.iowait;

    prev_active = prev_st.user + prev_st.nice + prev_st.system + prev_st.irq + prev_st.softirq + prev_st.steal;
    curr_active = curr_st.user + curr_st.nice + curr_st.system + curr_st.irq + curr_st.softirq + curr_st.steal;

    prev_total = prev_idle + prev_active;
    curr_total = curr_idle + curr_active;

    total_delta = curr_total - prev_total;
    idle_delta = curr_idle - prev_idle;

    // * 100 --> result is in %
    return (int) (((total_delta - idle_delta) * 100) / total_delta);
}
