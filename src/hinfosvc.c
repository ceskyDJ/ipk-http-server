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
#include <time.h>

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
 * Maximum length of the first line of the HTTP request.
 * It is based on supported requests from project assignment
 */
#define MAX_MSG_LINE_LEN 30
/**
 * Maximum length of response message (header + body).
 * It is based on items' limits and the header skeleton
 */
#define OUTPUT_BUFFER_LEN 512
/**
 * Maximum length of HTTP' state message (based on the length of the longest HTTP state name)
 */
#define HTTP_STATE_MSG_LEN 26
/**
 * Maximum length of supported HTTP method => strlen("GET")
 */
#define HTTP_METHOD_LEN 3
/**
 * Maximum length of HTTP version => strlen("HTTP/1.1")
 */
#define HTTP_VERSION_LEN 8
/**
 * Maximum length of supported HTTP URI
 */
#define HTTP_URI_LEN (MAX_MSG_LINE_LEN - HTTP_METHOD_LEN - HTTP_VERSION_LEN)
/**
 * Maximum length of datetime formatted for HTTP headers => strlen("Tue, 22 Feb 2022 21:22:19 GMT")
 */
#define HTTP_DATETIME_LEN 29

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
    return (int) (((total_delta - idle_delta) * 100) / total_delta);
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
    if (setsockopt(welcome_socket, IPPROTO_IPV6, IPV6_V6ONLY, &(int) {0}, sizeof(int)) == -1) {
        fprintf(stderr, "Cannot setup socket\n");
        close(welcome_socket);
        return -1;
    }
    if (setsockopt(welcome_socket, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == -1) {
        fprintf(stderr, "Cannot setup socket\n");
        close(welcome_socket);
        return -1;
    }
    if (setsockopt(welcome_socket, SOL_SOCKET, SO_REUSEPORT, &(int) {1}, sizeof(int)) == -1) {
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
 * Constructs and returns current datetime in HTTP's header format
 *
 * @param formatted_datetime Pointer to the place where to save (return) the datetime
 */
void get_http_datetime(char *formatted_datetime) {
    time_t epoch_time;
    struct tm *gmt;

    time(&epoch_time);
    gmt = gmtime(&epoch_time);

    strftime(formatted_datetime, HTTP_DATETIME_LEN, "%a, %d %b %Y %H:%M:%S ", gmt);
}

/**
 * Goes throw buffer and increments its index until whitespaces are read
 *
 * @param buffer Buffer to work with
 * @param index Index of the buffer (pointer to it)
 */
void skip_whitespaces(const char *buffer, unsigned *index) {
    while (isspace(buffer[(*index)++])) {
        ; // Just skipping whitespace characters
    }

    // The last tries char wasn't whitespace, it needs to be recovered
    (*index)--;
}

/**
 * Parses HTTP request and retrieves parsed data
 *
 * @param http_request Buffer with the first line of the HTTP request
 * @param method Pointer to the place where to save parsed HTTP method
 * @param uri Pointer to the place where to save parsed HTTP URI
 * @param version Pointer to the place where to save parsed HTTP version
 * @return Error code (equals to HTTP error code number --> 200 => success, etc.)
 */
unsigned parse_http_request(const char *http_request, char *method, char *uri, char *version) {
    unsigned local_ix;
    unsigned req_ix = 0;

    // HTTP method
    for (local_ix = 0; local_ix < HTTP_METHOD_LEN; local_ix++) {
        method[local_ix] = http_request[req_ix++];
    }
    method[HTTP_METHOD_LEN] = '\0';

    if (strcmp(method, "GET") != 0) {
        // Forbidden method
        return 405;
    }

    skip_whitespaces(http_request, &req_ix);

    // HTTP URI
    for (local_ix = 0; local_ix < HTTP_URI_LEN; local_ix++) {
        if (!isspace(http_request[req_ix])) {
            uri[local_ix] = http_request[req_ix++];
        } else {
            // Whitespace char mean the end of the URI item
            break;
        }
    }
    memset(&uri[local_ix], '\0', HTTP_URI_LEN - strlen(uri));

    if (!isspace(http_request[req_ix])) {
        // HTTP URI is longer than maximum
        return 414;
    }

    skip_whitespaces(http_request, &req_ix);

    // HTTP version
    for (local_ix = 0; local_ix < HTTP_VERSION_LEN; local_ix++) {
        version[local_ix] = http_request[req_ix++];
    }

    if (strcmp(version, "HTTP/1.1") != 0) {
        // Unsupported HTTP version
        return 505;
    }

    return 200;
}

/**
 * Processed single HTTP request and prepares a response for it
 *
 * @param http_request Buffer with the first line of the HTTP request to process
 * @param http_response Buffer where to save complete HTTP response
 */
void process_http_request(const char *http_request, char *http_response) {
    char method[HTTP_METHOD_LEN + 1] = "";
    char uri[HTTP_URI_LEN + 1] = "";
    char version[HTTP_VERSION_LEN + 1] = "";

    unsigned status_code;
    char status_msg[HTTP_STATE_MSG_LEN + 1] = "OK";
    char datetime[HTTP_DATETIME_LEN + 1];
    char data[HOSTNAME_LENGTH + 1] = "";
    char response_body[HOSTNAME_LENGTH + 1] = ""; // Hostname is the longest possible body type

    // Parse HTTP request
    status_code = parse_http_request(http_request, method, uri, version);

    // Process parsed data
    if (status_code == 405) {
        sprintf(status_msg, "Method Not Allowed");
    } else if (status_code == 505) {
        sprintf(status_msg, "HTTP Version Not Supported");
    } else if (status_code == 414) {
        sprintf(status_msg, "URI Too Long");
    } else {
        if (strcmp(uri, "/hostname") == 0) {
            get_hostname(data);
            sprintf(response_body, "%s\r\n", data);
        } else if (strcmp(uri, "/cpu-name") == 0) {
            get_cpu_info(data);
            sprintf(response_body, "%s\r\n", data);
        } else if (strcmp(uri, "/load") == 0) {
            sprintf(response_body, "%d%%\r\n", get_cpu_load());
        } else {
            status_code = 404;
            sprintf(status_msg, "Not Found");
        }
    }

    // Construct response
    get_http_datetime(datetime);

    sprintf(http_response,
            "HTTP/1.1 %d %s\r\n"
            "Connection: close\r\n"
            "Date: %s\r\n"
            "Server: hinfosvc/1.0\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "%s", status_code, status_msg, datetime, (int)strlen(response_body), response_body);
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
    char request_buffer[MAX_MSG_LINE_LEN + 1] = "";
    char response_buffer[OUTPUT_BUFFER_LEN + 1] = "";

    // Load port from CLI (required argument)
    if (argc < 2) {
        fprintf(stderr, "You need to specify a port. For example: %s 12345\n", argv[0]);
        return 1;
    }

    port = strtoul(argv[1], NULL, 10);
    if (port < 1025 || port > 65535) {
        fprintf(stderr, "Port must be a number 1025-65535 (0-1024 are protected by OS)\n");
        return 1;
    }

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
        // Prepare file descriptor watchers
        FD_ZERO(&read_socks);
        FD_ZERO(&write_socks);

        FD_SET(welcome_socket, &read_socks);
        FD_SET(int_signal, &read_socks);
        FD_SET(welcome_socket, &write_socks);

        // Passive wait for new connection or SIGINT
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

        // Get HTTP request
        if ((int) read(conn_socket, request_buffer, sizeof(request_buffer) - 1) == -1) {
            fprintf(stderr, "Cannot read data from connection socket\n");
            close(conn_socket);
            close(welcome_socket);
            return 1;
        }

        process_http_request(request_buffer, response_buffer);

        // Send HTTP response
        if (write(conn_socket, response_buffer, strlen(response_buffer)) == -1) {
            fprintf(stderr, "Cannot write data to connection socket\n");
            close(conn_socket);
            close(welcome_socket);
            return 1;
        }

        if (close(conn_socket) == -1) {
            fprintf(stderr, "Cannot close connection socket\n");
            close(welcome_socket);
            return 1;
        }
    }

    return 0;
}
