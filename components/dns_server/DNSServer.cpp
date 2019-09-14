#include "DNSServer.h"
#include <lwip/def.h>
#include <lwip/ip_addr.h>
// #include <Arduino.h>
#include <cctype>
#include <esp_log.h>
#include <memory>
#include <string.h>

#define DNS_HEADER_SIZE sizeof(DNSHeader)

static const char *TAG = "dns_server";

void DNSServer::task(void *parm) {
    DNSServer *server = (DNSServer *)parm;
    for (;;) {
        server->processNextRequest();
    }
    vTaskDelete(NULL);
}

DNSServer::DNSServer() {
    _ttl = lwip_htonl(60);
    _errorReplyCode = DNSReplyCode::NonExistentDomain;
    _task = NULL;
}

bool DNSServer::start(const uint16_t &port, std::string &domainName,
                      const ip_addr_t &resolvedIP) {
    _port = port;

    _domainName = domainName;
    _resolvedIP[0] = ip4_addr1(&resolvedIP);
    _resolvedIP[1] = ip4_addr2(&resolvedIP);
    _resolvedIP[2] = ip4_addr3(&resolvedIP);
    _resolvedIP[3] = ip4_addr4(&resolvedIP);
    downcaseAndRemoveWwwPrefix(_domainName);
    ESP_LOGI(TAG,
             "Starting at port: %d, with domainName: %s and ip: %d.%d.%d.%d",
             _port, _domainName.c_str(), _resolvedIP[0], _resolvedIP[1],
             _resolvedIP[2], _resolvedIP[3]);
    _udp = netconn_new(NETCONN_UDP);
    if (netconn_bind(_udp, IP_ADDR_ANY, _port) != ERR_OK) return false;
    xTaskCreate(DNSServer::task, "DNS_SERVER_TASK", 1024, this, 9, &_task);
    return true;
}

void DNSServer::setErrorReplyCode(const DNSReplyCode &replyCode) {
    _errorReplyCode = replyCode;
}

void DNSServer::setTTL(const uint32_t &ttl) { _ttl = lwip_htonl(ttl); }

void DNSServer::stop() {
    if (_task != NULL) {
        vTaskDelete(_task);
        _task = NULL;
    }
    netconn_delete(_udp);
}

void DNSServer::downcaseAndRemoveWwwPrefix(std::string &domainName) {
    for (char &c : domainName) {
        c = std::tolower(c);
    }
    if (domainName.rfind("www.", 0) == 0) {
        domainName.erase(0, 4);
    }
}

void DNSServer::respondToRequest(DNSPacket *dnsPacket, size_t length) {
    DNSHeader *dnsHeader;
    uint8_t *query, *start;
    const char *matchString;
    size_t remaining, labelLength, queryLength;
    uint16_t qtype, qclass;

    dnsHeader = dnsPacket->dnsHeader;

    // Must be a query for us to do anything with it
    if (dnsHeader->QR != DNS_QR_QUERY) {
        ESP_LOGI(TAG, "Not a query, ignored request");
        return;
    }

    // If operation is anything other than query, we don't do it
    if (dnsHeader->OPCode != DNS_OPCODE_QUERY) {
        ESP_LOGI(
            TAG,
            "Operation %d is not a query, reply with not implemented error",
            dnsHeader->OPCode);
        return replyWithError(dnsPacket, DNSReplyCode::NotImplemented);
    }

    // Only support requests containing single queries - everything else
    // is badly defined
    if (dnsHeader->QDCount != lwip_htons(1)) {
        ESP_LOGI(TAG, "More than 1 queries are not supported");
        return replyWithError(dnsPacket, DNSReplyCode::FormError);
    }

    // We must return a FormError in the case of a non-zero ARCount to
    // be minimally compatible with EDNS resolvers
    if (dnsHeader->ANCount != 0 || dnsHeader->NSCount != 0 ||
        dnsHeader->ARCount != 0)
        return replyWithError(dnsPacket, DNSReplyCode::FormError);

    // Even if we're not going to use the query, we need to parse it
    // so we can check the address type that's being queried

    uint8_t *buffer = (uint8_t *)dnsPacket->dnsHeader;
    query = start = buffer + DNS_HEADER_SIZE;
    remaining = length - DNS_HEADER_SIZE;
    while (remaining != 0 && *start != 0) {
        labelLength = *start;
        if (labelLength + 1 > remaining)
            return replyWithError(dnsPacket, DNSReplyCode::FormError);
        remaining -= (labelLength + 1);
        start += (labelLength + 1);
    }

    // 1 octet labelLength, 2 octet qtype, 2 octet qclass
    if (remaining < 5)
        return replyWithError(dnsPacket, DNSReplyCode::FormError);

    start += 1; // Skip the 0 length label that we found above

    memcpy(&qtype, start, sizeof(qtype));
    start += 2;
    memcpy(&qclass, start, sizeof(qclass));
    start += 2;

    queryLength = start - query;

    ESP_LOGI(TAG, "Query: %s", query);

    if (qclass != lwip_htons(DNS_QCLASS_ANY) &&
        qclass != lwip_htons(DNS_QCLASS_IN))
        return replyWithError(dnsPacket, DNSReplyCode::NonExistentDomain, query,
                              queryLength);

    if (qtype != lwip_htons(DNS_QTYPE_A) && qtype != lwip_htons(DNS_QTYPE_ANY))
        return replyWithError(dnsPacket, DNSReplyCode::NonExistentDomain, query,
                              queryLength);

    // If we have no domain name configured, just return an error
    if (_domainName == "")
        return replyWithError(dnsPacket, _errorReplyCode, query, queryLength);

    // If we're running with a wildcard we can just return a result now
    if (_domainName == "*") {
        ESP_LOGI(TAG, "Wildcard response");
        return replyWithIP(dnsPacket, query, queryLength);
    }

    matchString = _domainName.c_str();

    start = query;

    // If there's a leading 'www', skip it
    if (*start == 3 && strncasecmp("www", (char *)start + 1, 3) == 0)
        start += 4;

    while (*start != 0) {
        labelLength = *start;
        start += 1;
        while (labelLength > 0) {
            if (tolower(*start) != *matchString)
                return replyWithError(dnsPacket, _errorReplyCode, query,
                                      queryLength);
            ++start;
            ++matchString;
            --labelLength;
        }
        if (*start == 0 && *matchString == '\0')
            return replyWithIP(dnsPacket, query, queryLength);

        if (*matchString != '.')
            return replyWithError(dnsPacket, _errorReplyCode, query,
                                  queryLength);
        ++matchString;
    }

    return replyWithError(dnsPacket, _errorReplyCode, query, queryLength);
}

void DNSServer::processNextRequest() {
    netbuf *buf = netbuf_new();
    netconn_recv(_udp, &buf);

    size_t currentPacketSize;

    currentPacketSize = netbuf_len(buf);
    if (currentPacketSize == 0) return;

    // The DNS RFC requires that DNS packets be less than 512 bytes in size,
    // so just discard them if they are larger
    if (currentPacketSize > MAX_DNS_PACKETSIZE) return;

    // If the packet size is smaller than the DNS header, then someone is
    // messing with us
    if (currentPacketSize < DNS_HEADER_SIZE) return;

    uint8_t buffer[currentPacketSize];
    netbuf_copy(buf, buffer, currentPacketSize);
    DNSPacket *dnsPacket = new DNSPacket();
    dnsPacket->dnsHeader = (DNSHeader *)buffer;
    dnsPacket->addr = buf->addr;
    dnsPacket->port = buf->port;
    netbuf_free(buf);
    ESP_LOGI(TAG, "Received DNS request");
    respondToRequest(dnsPacket, currentPacketSize);
}

void DNSServer::writeNBOShort(netbuf *buf, uint16_t value, uint16_t &offset) {
    pbuf_take_at(buf->p, &value, sizeof(value), offset);
    offset += sizeof(value);
}

void DNSServer::replyWithIP(DNSPacket *dnsPacket, unsigned char *query,
                            size_t queryLength) {
    uint16_t value;

    DNSHeader *dnsHeader = dnsPacket->dnsHeader;
    dnsHeader->QR = DNS_QR_RESPONSE;
    dnsHeader->QDCount = lwip_htons(1);
    dnsHeader->ANCount = lwip_htons(1);
    dnsHeader->NSCount = 0;
    dnsHeader->ARCount = 0;

    // _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    // _udp.write((unsigned char *)dnsHeader, sizeof(DNSHeader));
    // _udp.write(query, queryLength);
    netbuf *buf = netbuf_new();
    netbuf_alloc(buf, DNS_HEADER_SIZE + queryLength + 2 * 4 + sizeof(_ttl) +
                          sizeof(_resolvedIP));
    u16_t offset = 0;
    pbuf_take(buf->p, dnsHeader, DNS_HEADER_SIZE);
    offset += DNS_HEADER_SIZE;
    free(dnsHeader);
    pbuf_take_at(buf->p, query, queryLength, offset);
    offset += queryLength;
    free(query);
    // Rather than restate the name here, we use a pointer to the name contained
    // in the query section. Pointers have the top two bits set.
    value = 0xC000 | DNS_HEADER_SIZE;
    writeNBOShort(buf, lwip_htons(value), offset);

    // Answer is type A (an IPv4 address)
    writeNBOShort(buf, lwip_htons(DNS_QTYPE_A), offset);

    // Answer is in the Internet Class
    writeNBOShort(buf, lwip_htons(DNS_QCLASS_IN), offset);

    // Output TTL (already NBO)
    // _udp.write((unsigned char *)&_ttl, 4);
    pbuf_take_at(buf->p, &_ttl, sizeof(_ttl), offset);
    offset += sizeof(_ttl);

    // Length of RData is 4 bytes (because, in this case, RData is IPv4)
    writeNBOShort(buf, lwip_htons(sizeof(_resolvedIP)), offset);
    // _udp.write(_resolvedIP, sizeof(_resolvedIP));
    pbuf_take_at(buf->p, &_resolvedIP, sizeof(_resolvedIP), offset);
    // _udp.endPacket();
    netconn_sendto(_udp, buf, &dnsPacket->addr, dnsPacket->port);
    netbuf_free(buf);
    free(dnsPacket);
}

void DNSServer::replyWithError(DNSPacket *dnsPacket, DNSReplyCode rcode,
                               unsigned char *query, size_t queryLength) {
    DNSHeader *dnsHeader = dnsPacket->dnsHeader;
    dnsHeader->QR = DNS_QR_RESPONSE;
    dnsHeader->RCode = (unsigned char)rcode;
    if (query)
        dnsHeader->QDCount = lwip_htons(1);
    else
        dnsHeader->QDCount = 0;
    dnsHeader->ANCount = 0;
    dnsHeader->NSCount = 0;
    dnsHeader->ARCount = 0;
    netbuf *buf = netbuf_new();
    netbuf_alloc(buf, DNS_HEADER_SIZE + queryLength);
    // _udp.write((unsigned char *)dnsHeader, sizeof(DNSHeader));
    // if (query != NULL) _udp.write(query, queryLength);
    // _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    // _udp.endPacket();
    pbuf_take_at(buf->p, dnsHeader, DNS_HEADER_SIZE, 0);
    free(dnsHeader);
    if (query != NULL)
        pbuf_take_at(buf->p, query, queryLength, DNS_HEADER_SIZE);
    free(query);
    netconn_sendto(_udp, buf, &dnsPacket->addr, dnsPacket->port);
    netbuf_free(buf);
    free(dnsPacket);
}

void DNSServer::replyWithError(DNSPacket *dnsPacket, DNSReplyCode rcode) {
    replyWithError(dnsPacket, rcode, NULL, 0);
}
