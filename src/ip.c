#include "ip.h"

#include "arp.h"
#include "ethernet.h"
#include "icmp.h"
#include "net.h"

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac) {
    if (buf->len < sizeof(ip_hdr_t)) {
        return;
    }

    ip_hdr_t *ip_hdr = (ip_hdr_t *)buf->data;
    if (ip_hdr->version != 4 || buf->len < swap16(ip_hdr->total_len16)) {
        return;
    }

    uint16_t checksum = swap16(ip_hdr->hdr_checksum16);
    ip_hdr->hdr_checksum16 = 0;
    if (checksum != checksum16((uint16_t *)ip_hdr, ip_hdr->hdr_len * IP_HDR_LEN_PER_BYTE)) {
        return;
    }
    ip_hdr->hdr_checksum16 = swap16(checksum);

    if (memcmp(ip_hdr->dst_ip, net_if_ip, sizeof(net_if_ip)) != 0) {
        return;
    }

    if (buf->len > swap16(ip_hdr->total_len16)) {
        buf_remove_padding(buf, buf->len - swap16(ip_hdr->total_len16));
    }

    buf_remove_header(buf, sizeof(ip_hdr_t));
    if (net_in(buf, ip_hdr->protocol, ip_hdr->src_ip) < 0) {
        buf_add_header(buf, sizeof(ip_hdr_t));
        icmp_unreachable(buf, ip_hdr->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
    }
}
/**
 * @brief 处理一个要发送的ip分片
 *
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf) {
    buf_add_header(buf, sizeof(ip_hdr_t));
    ip_hdr_t *ip_hdr = (ip_hdr_t *)buf->data;
    uint16_t ip_more_fragment = mf ? IP_MORE_FRAGMENT : 0;

    ip_hdr->version = IP_VERSION_4;
    ip_hdr->hdr_len = sizeof(ip_hdr_t) / IP_HDR_LEN_PER_BYTE;
    ip_hdr->tos = 0;
    ip_hdr->total_len16 = swap16(buf->len);
    // ip_hdr->id16 = swap16(id);
    ip_hdr->id16 = 0;
    ip_hdr->flags_fragment16 = swap16(ip_more_fragment | (offset >> 3));
    ip_hdr->ttl = 64;
    ip_hdr->protocol = protocol;
    ip_hdr->hdr_checksum16 = 0;
    memcpy(ip_hdr->src_ip, net_if_ip, sizeof(ip_hdr->src_ip));
    memcpy(ip_hdr->dst_ip, ip, sizeof(ip_hdr->dst_ip));

    ip_hdr->hdr_checksum16 = swap16(checksum16((uint16_t *)ip_hdr, sizeof(ip_hdr_t)));
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 *
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol) {
    buf_t ip_buf;
    size_t max_payload_len = ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t);
    int cnt = 0;
    uint16_t offset = 0;

    if (buf->len > max_payload_len) {
        while (buf->len > max_payload_len) {
            buf_init(&ip_buf, max_payload_len);
            memcpy(ip_buf.data, buf->data, max_payload_len);
            buf_remove_header(buf, max_payload_len);
            ip_fragment_out(&ip_buf, ip, protocol, cnt, offset, 1);
            offset += max_payload_len;
            cnt++;
        }
    }
    ip_fragment_out(buf, ip, protocol, cnt, offset, 0);
}

/**
 * @brief 初始化ip协议
 *
 */
void ip_init() {
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}