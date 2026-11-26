#ifndef TLS_H
#define TLS_H
#include <stdbool.h>

bool send_http_post(const char *host, uint16_t port, const char *path, const char *auth, const char *json_body);

#endif