#include "icmp.h"

#include "ip.h"
#include "net.h"

/**
 * @brief 发送icmp响应
 *
 * @param req_buf 收到的icmp请求包
 * @param src_ip 源ip地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip) {
    buf_init(&txbuf, req_buf->len);
    icmp_hdr_t *req_icmp_hdr = (icmp_hdr_t *)req_buf->data;
    icmp_hdr_t *icmp_hdr = (icmp_hdr_t *)txbuf.data;
    memcpy(txbuf.data, req_buf->data, req_buf->len);

    // buf_remove_header(req_buf, sizeof(icmp_hdr_t));
    // buf_add_header(&txbuf, sizeof(icmp_hdr_t));
    memset(icmp_hdr, 0, sizeof(icmp_hdr_t));
    icmp_hdr->type = ICMP_TYPE_ECHO_REPLY;
    icmp_hdr->id16 = req_icmp_hdr->id16;
    icmp_hdr->seq16 = req_icmp_hdr->seq16;
    icmp_hdr->checksum16 = swap16(checksum16((uint16_t *)txbuf.data, txbuf.len));

    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip) {
    if (buf->len < sizeof(icmp_hdr_t)) {
        return;
    }
    icmp_hdr_t *icmp_hdr = (icmp_hdr_t *)buf->data;
    if (icmp_hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        icmp_resp(buf, src_ip);
    }
}

/**
 * @brief 发送icmp不可达
 *
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code) {
    ip_hdr_t *recv_ip_hdr = (ip_hdr_t *)recv_buf->data;
    size_t icmp_data_len = recv_ip_hdr->hdr_len * IP_HDR_LEN_PER_BYTE + 8;

    buf_init(&txbuf, sizeof(icmp_hdr_t) + icmp_data_len);
    memcpy(txbuf.data + sizeof(icmp_hdr_t), recv_buf->data, icmp_data_len);

    icmp_hdr_t *icmp_hdr = (icmp_hdr_t *)txbuf.data;
    memset(icmp_hdr, 0, sizeof(icmp_hdr_t));

    icmp_hdr->type = ICMP_TYPE_UNREACH;
    icmp_hdr->code = code;
    icmp_hdr->checksum16 = swap16(checksum16((uint16_t *)txbuf.data, txbuf.len));

    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 初始化icmp协议
 *
 */
void icmp_init() {
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}