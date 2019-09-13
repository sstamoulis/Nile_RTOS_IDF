#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <lwip/ip_addr.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct DNSServer DNSServer;
typedef struct DNSServerWrap {
    DNSServer *dnsServer;
    TaskHandle_t dnsServerTaskHandle;
} DNSServerWrap_t;

DNSServerWrap_t dns_server_init();
void dns_server_delete(DNSServerWrap_t *server);
void dns_server_start(DNSServerWrap_t *server, uint16_t port,
                      const char *domainName, const ip_addr_t *resolvedIP);
void dns_server_stop(DNSServerWrap_t *server);
#ifdef __cplusplus
}
#endif