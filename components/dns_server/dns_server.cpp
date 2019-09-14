#include "dns_server.h"
#include "DNSServer.h"

extern "C" {

DNSServer *dns_server_init() { return new DNSServer(); }

void dns_server_delete(DNSServer *server) {
    dns_server_stop(server);
    delete server;
}

void dns_server_start(DNSServer *server, uint16_t port, const char *domainName,
                      const ip_addr_t *resolvedIP) {
    std::string strDomainName = std::string(domainName);
    server->start(port, strDomainName, *resolvedIP);
}

void dns_server_stop(DNSServer *server) {
    server->stop();
}
}