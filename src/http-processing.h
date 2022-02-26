#ifndef HINFOSVC_PROCESSING_H
#define HINFOSVC_PROCESSING_H
/**
 * @file http-processing.h
 * Header of HTTP processor
 *
 * @author Michal Šmahel (xsmahe01)
 */

/**
 * Maximum length of the first line of the HTTP request.
 * It is based on supported requests from project assignment
 */
#define MAX_MSG_LINE_LEN 30
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
 * Processed single HTTP request and prepares a response for it
 *
 * @param http_request Buffer with the first line of the HTTP request to process
 * @param http_response Buffer where to save complete HTTP response
 */
void process_http_request(const char *http_request, char *http_response);

#endif //HINFOSVC_PROCESSING_H
