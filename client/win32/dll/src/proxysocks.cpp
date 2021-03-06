#include "stdafx.h"
#include <windows.h> 
#include <Winsock2.h>

#include "commtype.h"
#include "proxysocks.h"
#include "procCommClient.h"
#include "socketwrap.h"
#include "entry.h"
#include "hooksock.h"

static uint32_t g_proxy_socks_cnt;
static proxy_sock_t g_proxy_socks[PROXY_SOCK_MAX_FD];

static const char* g_proxy_sock_status_desc[] =
{
    "INVALID",
    "INIT",
    "REGISTERING",
    "PROXING",
    "ABNORMAL",
};

static void _enter_state(proxy_sock_t *proxy_sock, SOCKS_STATUS_E status)
{
	if (proxy_sock->status == status)
    {
        _LOG_ERROR("proxy sock %d already in %s", proxy_sock->sockfd, 
        	g_proxy_sock_status_desc[proxy_sock->status]);
        return;
    }

    proxy_sock->status = status;
    _LOG_INFO("proxy sock %d enter %s", proxy_sock->sockfd, 
        	g_proxy_sock_status_desc[proxy_sock->status]);
    return;
}

static int _send_register(int sockfd, uint32_t remote_addr, uint16_t remote_port)
{
    int ret = 0;
    
    register_req_t reg_req;
    reg_req.magic = htonl(PROXY_MSG_MAGIC);
    reg_req.proc_id = g_cur_proc_pid;
    reg_req.remote_ipaddr = htonl(remote_addr);
    reg_req.remote_port = htons(remote_port);

    ret = origin_sock_write_timeout(sockfd, (char*)&reg_req, sizeof(reg_req), -1);
    if (ret != sizeof(reg_req))
    {
        _LOG_ERROR("proxy sock %d send register req to proxy server failed, remote 0x%x/%d", sockfd, remote_addr, remote_port);
        return -1;
    }

    register_resp_t reg_resp;
    ret = sock_read_timeout(sockfd, (char*)&reg_resp, sizeof(reg_resp), -1);
    if (ret != sizeof(reg_resp))
    {
        _LOG_ERROR("proxy sock %d get register resp from proxy server failed, remote 0x%x/%d", sockfd, remote_addr, remote_port);
        return -1;
    }

    if (reg_resp.result == false)
    {
    	_LOG_WARN("proxy sock %d register to proxy server failed, remote 0x%x/%d", sockfd, remote_addr, remote_port);	
    	return -1;
    }

    _LOG_INFO("proxy sock %d register to proxy server success, remote 0x%x/%d", sockfd, remote_addr, remote_port);
    return 0;
}

static proxy_sock_t* proxysocks_get(int sockfd)
{
	proxy_sock_t *proxy_sock = &g_proxy_socks[sockfd];
	if (proxy_sock->sockfd != sockfd)
	{
		return NULL;
	}
    return proxy_sock;
}

static int proxysocks_add(int sockfd, int remote_addr, int remote_port)
{
	proxy_sock_t *new_sock = &g_proxy_socks[sockfd];
	new_sock->sockfd = sockfd;
	new_sock->remote_addr = remote_addr;
	new_sock->remote_port = remote_port;
	_enter_state(new_sock, SOCKS_INIT);

    g_proxy_socks_cnt++;
    _LOG_INFO("add new proxy sock %d, remote 0x%x/%d", sockfd, remote_addr, remote_port);
    return 0;
}

static void proxysocks_del(int sockfd)
{
	proxy_sock_t *pSock = NULL;

	pSock = proxysocks_get(sockfd);
	if (NULL == pSock)
	{
		//maybo not need proxy
		//_LOG_WARN("proxy sock %d not exist when del", sockfd);
		return;
	}
    g_proxy_socks_cnt--;    
	pSock->sockfd = 0;
	pSock->status = 0;

	_LOG_INFO("del proxy sock %d", sockfd);
	return;
}

int proxysocks_connect_handle(int sockfd, int remote_addr, int remote_port)
{
	if (sockfd >= PROXY_SOCK_MAX_FD)
	{
		_LOG_WARN("sockfd %d too large when connect", sockfd);
		return 0;
	}
	
	return proxysocks_add(sockfd, remote_addr, remote_port);
}

int proxysocks_close_handle(int sockfd)
{
	if (sockfd >= PROXY_SOCK_MAX_FD)
	{
		_LOG_WARN("sockfd %d too large when close", sockfd);
		return 0;
	}
	
	proxysocks_del(sockfd);
	return 0;
}

int proxysocks_send_handle(int sockfd, uint32_t bytes)
{
	if (sockfd >= PROXY_SOCK_MAX_FD)
	{
		_LOG_WARN("sockfd %d too large when send", sockfd);
		return 0;
	}

	proxy_sock_t *pSock = NULL;
	int remote_addr = 0;
	unsigned short remote_port = 0;

	pSock = proxysocks_get(sockfd);
	if (NULL == pSock)
	{
		return 0;
	}
	switch(pSock->status)
	{
		case SOCKS_INIT:
			remote_addr = pSock->remote_addr;
			remote_port = pSock->remote_port;
			_enter_state(pSock, SOCKS_REGISTERING);

			/*register firstly*/	
			if(0 != _send_register(sockfd, remote_addr, remote_port))
			{
				_enter_state(pSock, SOCKS_ABNORMAL);
				return -1;
			}

			_enter_state(pSock, SOCKS_PROXING);
			break;
		case SOCKS_REGISTERING:
			_LOG_WARN("proxy sock %d already in registering when send, wait", sockfd);
			return -1;
			break;
		case SOCKS_PROXING:
			break;
		case SOCKS_ABNORMAL:
			_LOG_WARN("proxy sock %d in ABNORMAL when send", sockfd);
			return -1;
			break;
		default:
			_LOG_ERROR("proxy sock %d in invalid status %d when send", sockfd, pSock->status);
			return -1;
			break;
	}
	return 0;
}

int proxysocks_init()
{
	g_proxy_socks_cnt = 0;
    memset(g_proxy_socks, 0, sizeof(g_proxy_socks));

	_LOG_INFO("proxysocks init ok");
	return 0;
}

void proxysocks_free()
{
	for (int ii = 0; ii < PROXY_SOCK_MAX_FD; ii++)
	{
		if (g_proxy_socks[ii].sockfd != 0)
		{
			closesocket(g_proxy_socks[ii].sockfd);
			g_proxy_socks[ii].sockfd = 0;
		}
	}
	return;
}
