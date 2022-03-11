/**
 * @file hinfosvc.c
 * Main module of HTTP server providing some system info (1st IPK project)
 *
 * @author Michal Šmahel (xsmahe01)
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include "http-processing.h"

/**
 * Maximum length of response message (header + body).
 * It is based on items' limits and the header skeleton
 */
#define OUTPUT_BUFFER_LEN 512

/**
 * Creates and inits the welcome socket for TCP/IP communication
 *
 * @param port Port to bind socket to
 * @return Welcome socket file descriptor or -1 if error occurred
 * @pre Valid port number (1025-65535)
 */
int make_welcome_socket(unsigned port) {
    int welcome_socket;
    struct sockaddr_in6 server_addr;
    int socket_flags;

    // Create a new socket
    if ((welcome_socket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        fprintf(stderr, "Cannot create socket\n");
        return -1;
    }

    // Apply configurations
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

        // Process HTTP request
        if (process_http_request(conn_socket, response_buffer) != 0) {
            fprintf(stderr, "Cannot process HTTP request\n");
            close(conn_socket);
            close(welcome_socket);
            return 1;
        }

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
