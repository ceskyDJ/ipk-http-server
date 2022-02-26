/**
 * @file http-processing.c
 * Processor of HTTP requests
 *
 * @author Michal Šmahel (xsmahe01)
 */
#include <bits/types/time_t.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "http-processing.h"
#include "system-info.h"

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

    strftime(formatted_datetime, HTTP_DATETIME_LEN, "%a, %d %b %Y %H:%M:%S", gmt);
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

    // There should be at least one whitespace we need to skip
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

    // There should be at least one whitespace we need to skip
    skip_whitespaces(http_request, &req_ix);

    // HTTP version
    for (local_ix = 0; local_ix < HTTP_VERSION_LEN; local_ix++) {
        version[local_ix] = http_request[req_ix++];
    }

    if (strcmp(version, "HTTP/1.1") != 0) {
        // Unsupported HTTP version
        return 505;
    }

    // OK
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
    char response_body[HOSTNAME_LENGTH + 1 + 2] = ""; // Hostname is the longest possible body type, \r\n --> +2

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
        // status_code == 200
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
