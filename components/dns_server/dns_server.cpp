#include "DNSServer.h"
#include "dns_server.h"

extern "C" {

DNSServerWrap_t dns_server_init() {
    return {
        .dnsServer = new DNSServer(),
        .dnsServerTaskHandle = NULL,
    };
}

void dns_server_delete(DNSServerWrap_t *server) {
    dns_server_stop(server);
    delete server->dnsServer;
}

void dns_server_task(void *parm) {
    DNSServer *server = (DNSServer *)parm;
    for (;;) {
        server->processNextRequest();
    }
    vTaskDelete(NULL);
}

TaskHandle_t _dnsServerTaskHandle = NULL;
void dns_server_start(DNSServerWrap_t *server, uint16_t port,
                      const char *domainName, const ip_addr_t *resolvedIP) {
    std::string strDomainName = std::string(domainName);
    server->dnsServer->start(port, strDomainName, *resolvedIP);
    xTaskCreate(dns_server_task, "DNS_SERVER_TASK", 1024, server->dnsServer, 9,
                &server->dnsServerTaskHandle);
}

void dns_server_stop(DNSServerWrap_t *server) {
    if (server->dnsServerTaskHandle != NULL) {
        vTaskDelete(server->dnsServerTaskHandle);
        server->dnsServerTaskHandle = NULL;
    }
    server->dnsServer->stop();
}
}