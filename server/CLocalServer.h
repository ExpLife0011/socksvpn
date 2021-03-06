#ifndef _SRV_LOCALSERVER_H
#define _SRV_LOCALSERVER_H


class CLocalServer : public CNetRecv
{
public:
    CLocalServer(uint32_t ipaddr, uint16_t port) : CNetRecv(ipaddr, port)
    {
        this->set_thrd_index(0);

        memset(m_recv_buf, 0, sizeof(m_recv_buf));
        m_recv_len = 0;

        _LOG_INFO("create local server(peer %s:%u)", m_ipstr, m_port);
    }
    virtual ~CLocalServer()
    {
        _LOG_INFO("destroy local server(peer %s:%u)", m_ipstr, m_port);
    }

private:
    int pdu_handle(char *pdu_buf, int pdu_len);

    int connect_handle(BOOL result);
    int recv_handle(char *buf, int buf_len);
    void free_handle();

public:
    int send_register();
    int send_keepalive();

private:
    int msg_request_handle(PKT_R2S_HDR_T *r2shdr, char *data_buf, int data_len);
    int msg_client_close_handle(PKT_R2S_HDR_T *r2shdr, char *data_buf, int data_len);
    int msg_data_handle(PKT_R2S_HDR_T *r2shdr, char *data_buf, int data_len);

private:
    char m_recv_buf[2048];
    int m_recv_len;
};

extern MUTEX_TYPE m_local_srv_lock;
extern CLocalServer *g_LocalServ;
#endif
