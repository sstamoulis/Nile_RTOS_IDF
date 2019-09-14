#pragma once
#include <lwip/ip_addr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DNSServer DNSServer;

DNSServer *dns_server_init();
void dns_server_delete(DNSServer *server);
void dns_server_start(DNSServer *server, uint16_t port,
                      const char *domainName, const ip_addr_t *resolvedIP);
void dns_server_stop(DNSServer *server);
#ifdef __cplusplus
}
#endif