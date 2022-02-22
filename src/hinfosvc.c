#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/signalfd.h>

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
    return (int)(((total_delta - idle_delta) * 100) / total_delta);
}

/**
 * Creates and inits the welcome socket for TCP/IP communication
 *
 * @param port Port to bind socket to
 * @return Welcome socket file descriptor or -1 if error occurred
 */
int make_welcome_socket(unsigned port) {
    int welcome_socket;
    struct sockaddr_in6 server_addr;
    int socket_flags;

    if ((welcome_socket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        fprintf(stderr, "Cannot create socket\n");
        return -1;
    }

    // Source: https://stackoverflow.com/a/1618259
    if (setsockopt(welcome_socket, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0}, sizeof(int)) == -1) {
        fprintf(stderr, "Cannot setup socket\n");
        close(welcome_socket);
        return -1;
    }
    if (setsockopt(welcome_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        fprintf(stderr, "Cannot setup socket\n");
        close(welcome_socket);
        return -1;
    }
    if (setsockopt(welcome_socket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) == -1) {
        fprintf(stderr, "Cannot setup socket\n");
        close(welcome_socket);
        return -1;
    }

    // Activate non-blocking mode
    socket_flags = fcntl(welcome_socket, F_GETFL, 0);
    fcntl(welcome_socket, F_SETFL, socket_flags | O_NONBLOCK);

    // Assign an address and the port to the socket
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

    if (bind(welcome_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Cannot bind socket to port %d\n", port);
        close(welcome_socket);
        return -1;
    }

    return welcome_socket;
}

/**
 * Makes and inits SIGINT file descriptor
 *
 * @return SIGINT file descriptor or -1 if error occurred
 */
int make_int_sig_fd() {
    sigset_t signal_set;

    // Prepare mask
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);

    // Block standard signal handling
    if (sigprocmask(SIG_BLOCK, &signal_set, NULL) == -1) {
        fprintf(stderr, "Cannot apply signal mask\n");
        return -1;
    }

    return signalfd(-1, &signal_set, 0);
}

/**
 * Init (main) function of the program
 *
 * @param argc Number of CLI arguments
 * @param argv CLI arguments as array of "strings"
 * @return Program's exit code
 *
 * Inspired by the 2nd presentation from the subject IPK on FIT BUT
 */
int main(int argc, char *argv[]) {
    bool keep_running = true;
    unsigned port;

    int int_signal;
    int conn_socket;
    int welcome_socket;
    fd_set read_socks, write_socks;

    struct sockaddr_in6 client_addr;
    unsigned client_addr_len = sizeof(client_addr);
    char buffer[256] = "";
    int read_state;

    char hostname[HOSTNAME_LENGTH + 1] = "";
    char cpu_info[CPU_INFO_LENGTH + 1] = "";
    int cpu_load;

    // Load port from CLI (required argument)
    if (argc < 2) {
        fprintf(stderr, "You need to specify a port. For example: %s 12345\n", argv[0]);
        return 1;
    }

    port = strtoul(argv[1], NULL, 10);

    // Setup handling SIGINT for smooth stop of the program
    if ((int_signal = make_int_sig_fd()) == -1) {
        fprintf(stderr, "Cannot create SIGINT file descriptor\n");
        return 1;
    }

    // Setup socket
    if ((welcome_socket = make_welcome_socket(port)) == -1) {
        return 1;
    }

    // Start listening
    if (listen(welcome_socket, 1) == -1) {
        fprintf(stderr, "Cannot start socket listening\n");
        close(welcome_socket);
        return 1;
    }

    while (keep_running) {
        FD_ZERO(&read_socks);
        FD_ZERO(&write_socks);

        FD_SET(welcome_socket, &read_socks);
        FD_SET(int_signal, &read_socks);
        FD_SET(welcome_socket, &write_socks);

        select(FD_SETSIZE, &read_socks, &write_socks, NULL, NULL);

        // Handling SIGINT --> stop the server
        if (FD_ISSET(int_signal, &read_socks)) {
            close(welcome_socket);
            keep_running = false;
            continue;
        }

        if (!FD_ISSET(welcome_socket, &write_socks) && !FD_ISSET(welcome_socket, &read_socks)) {
            continue;
        }

        // Create connection for data transfer throw new socket
        conn_socket = accept(welcome_socket, (struct sockaddr *) &client_addr, &client_addr_len);
        if (conn_socket == -1) {
            fprintf(stderr, "Cannot create connection socket for data transfer\n");
            close(welcome_socket);
            return 1;
        }

        // We have connection --> we can read data from client
        while ((read_state = (int)read(conn_socket, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[strlen(buffer) - 1] = '\0';
            printf("%s\n", buffer);
        }

        // TODO: send response

        if (close(conn_socket) == -1) {
            fprintf(stderr, "Cannot close connection socket\n");
            close(welcome_socket);
            return 1;
        }

        if (read_state == -1) {
            fprintf(stderr, "Cannot read data from connection socket\n");
            close(welcome_socket);
            return 1;
        }
    }

    // Run the HTTP server
    if (get_hostname(hostname) != 0) {
        return 1;
    }
    if (get_cpu_info(cpu_info) != 0) {
        return 1;
    }
    if ((cpu_load = get_cpu_load()) < 0) {
        return 1;
    }

    printf("Hostname: %s\n", hostname);
    printf("CPU info: %s\n", cpu_info);
    printf("CPU load: %d%%\n", cpu_load);

    return 0;
}
