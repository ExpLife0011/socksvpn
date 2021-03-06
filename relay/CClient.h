#ifndef _RELAY_CLIENT_H
#define _RELAY_CLIENT_H

#include "relay_pkt_def.h"
#include "socks_relay.h"

class CClient : public CBaseClient
{
public:
    CClient(uint32_t ipaddr, uint16_t port, int fd, CBaseConnection *owner) : CBaseClient(ipaddr, port, fd, owner)
    {
        m_inner_ipaddr = 0;

        g_total_connect_req_cnt++;
        m_request_time = util_get_cur_time();
    }
    virtual ~CClient()
    {
    }
    
public:
    int init()
    {
        return 0;
    }
    void free()
    {
        this->free_handle();
    }
    int recv_handle(char *buf, int buf_len)
    {
        _LOG_ERROR("CClient in relay not recv");
        return -1;
    }
public:
    int send_remote_close_msg();
    int send_connect_result_msg(char *buf, int buf_len);
    int send_data_msg(char *buf, int buf_len);

private:
    uint64_t m_request_time;
};

#endif
