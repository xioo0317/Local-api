/* curl_config.h — minimal hand-written configuration for vendored curl.
 *
 * Target: Linux (host) + Android (NDK, LP64 ABIs).
 * HTTP only: TLS and every other protocol are disabled; DNS resolution
 * uses the threaded resolver. SIZEOF_* use compiler built-ins so both
 * LP64 (arm64-v8a, x86_64) and LP32 (armeabi-v7a) ABIs are correct.
 */
#ifndef CURL_CONFIG_H
#define CURL_CONFIG_H

/* ---------------- headers ---------------- */
#define HAVE_ARPA_INET_H 1
#define HAVE_ASSERT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MALLOC_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_POLL_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UN_H 1
#define HAVE_SYS_UTSNAME_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1

/* ---------------- functions ---------------- */
#define HAVE_CLOCK_GETTIME_MONOTONIC 1
#define HAVE_FCNTL 1
#define HAVE_FSEEKO 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETHOSTBYNAME 1
#define HAVE_GETHOSTNAME 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_IOCTL 1
#define HAVE_FCNTL_O_NONBLOCK 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_SELECT 1
#define HAVE_SIGACTION 1
#define HAVE_SIGNAL 1
#define HAVE_SNPRINTF 1
#define HAVE_SOCKET 1
#define HAVE_STRCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR_R 1
#if defined(__ANDROID__)
#define HAVE_POSIX_STRERROR_R 1
#define CURL_OS "Android"
#else
#define HAVE_GLIBC_STRERROR_R 1
#define CURL_OS "Linux"
#endif
#define HAVE_STRUCT_TIMEVAL 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_SA_FAMILY_T 1
#define HAVE_SOCKLEN_T 1
#define HAVE_MSG_NOSIGNAL 1
#define HAVE_RECV 1
#define HAVE_SEND 1
#define RECV_TYPE_ARG1 int
#define RECV_TYPE_ARG2 void *
#define RECV_TYPE_ARG3 size_t
#define RECV_TYPE_ARG4 int
#define RECV_TYPE_RETV ssize_t
#define SEND_TYPE_ARG1 int
#define SEND_QUAL_ARG2 const
#define SEND_TYPE_ARG2 void *
#define SEND_TYPE_ARG3 size_t
#define SEND_TYPE_ARG4 int
#define SEND_TYPE_RETV ssize_t
#define HAVE_STRNCASECMP 1

/* ---------------- types & sizes (LP64) ---------------- */
#define HAVE_BOOL_T 1
#define HAVE_LONGLONG 1
#define HAVE_LL 1
#define RETSIGTYPE void
#define SIZEOF_INT __SIZEOF_INT__
#define SIZEOF_LONG __SIZEOF_LONG__
#define SIZEOF_LONG_LONG __SIZEOF_LONG_LONG__
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#define SIZEOF_VOIDP __SIZEOF_POINTER__
#if defined(__LP64__)
#define SIZEOF_OFF_T 8
#define SIZEOF_TIME_T 8
#else
#define SIZEOF_OFF_T 4
#define SIZEOF_TIME_T 4
#endif
#define SIZEOF_CURL_OFF_T 8

/* ---------------- resolver ---------------- */
#define USE_THREADS_POSIX 1

/* ---------------- disabled protocols ---------------- */
#define CURL_DISABLE_DICT 1
#define CURL_DISABLE_FTP 1
#define CURL_DISABLE_GOPHER 1
#define CURL_DISABLE_IMAP 1
#define CURL_DISABLE_LDAP 1
#define CURL_DISABLE_LDAPS 1
#define CURL_DISABLE_MQTT 1
#define CURL_DISABLE_POP3 1
#define CURL_DISABLE_RTSP 1
#define CURL_DISABLE_SMB 1
#define CURL_DISABLE_SMTP 1
#define CURL_DISABLE_TELNET 1
#define CURL_DISABLE_TFTP 1

/* ---------------- disabled features ---------------- */
#define CURL_DISABLE_TLS 1
#define CURL_DISABLE_ALTSVC 1
#define CURL_DISABLE_AWS 1
#define CURL_DISABLE_COOKIES 1
#define CURL_DISABLE_CRYPTO_AUTH 1
#define CURL_DISABLE_DOH 1
#define CURL_DISABLE_HSTS 1
#define CURL_DISABLE_HTTP_AUTH 1
#define CURL_DISABLE_NETRC 1
#define CURL_DISABLE_SHUFFLE_DNS 1

#endif /* CURL_CONFIG_H */
