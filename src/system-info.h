#ifndef HINFOSVC_SYSTEM_INFO_H
#define HINFOSVC_SYSTEM_INFO_H
/**
 * @file system-info.h
 * Header file of system info provider
 *
 * @author Michal Šmahel (xsmahe01)
 */

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
 * Finds and returns hostname of the computer keep_running this program
 *
 * @param hostname Pointer to place where to save found hostname to
 * @return 0 => success, 1 => error
 * @pre hostname != NULL
 */
int get_hostname(char *hostname);

/**
 * Finds and returns computer's CPU info
 *
 * @param cpu_info Pointer to place where to save found cpu info
 * @return 0 => success, 1 => error
 * @pre cpu_info != NULL
 */
int get_cpu_info(char *cpu_info);

/**
 * Counts CPU load (for all CPU units)
 *
 * @return positive number => CPU load value in %, -1 => error
 *
 * Inspired by: https://stackoverflow.com/a/23376195
 */
int get_cpu_load(void);

#endif //HINFOSVC_SYSTEM_INFO_H
