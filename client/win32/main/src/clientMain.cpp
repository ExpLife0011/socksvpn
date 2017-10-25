
#include "common.h"
#include "clientMain.h"
#include "proxyConfig.h"
#include "procCommServ.h"
#include "procMgr.h"
#include "netpool.h"

#include <TlHelp32.h> 
#include <io.h>
#include <mswsock.h>

#include "common_def.h"
#include "utilcommon.h"
#include "CSocksMem.h"
#include "CLocalServer.h"
#include "CRemoteServer.h"
#include "CConnection.h"
#include "CConMgr.h"
#include "inject.h"
#include "CSyslogMgr.h"

char g_localModDir[MAX_PATH + 1] = { 0 };
HWND g_parentWnd;

static void _remote_chk_timer_callback(void* param1, void* param2,
                void* param3, void* param4)
{
	if(!g_is_start) return;
	
	MUTEX_LOCK(m_remote_srv_lock);
    if (NULL == g_RemoteServ)
    {
    	proxy_cfg_t* cfginfo = proxy_cfg_get();

        g_RemoteServ = new CRemoteServer(cfginfo->vpn_ip, cfginfo->vpn_port);
//        g_RemoteServ->init_async_write_resource(socks_malloc, socks_free);
        if(0 != g_RemoteServ->init())
        {
        	delete g_RemoteServ;
        	g_RemoteServ = NULL;
        }
    }
    else if (g_RemoteServ->is_connected())
    {
    	if (!g_RemoteServ->is_authed())
	    {
	    	g_RemoteServ->send_auth_quest_msg();
	    }
	    else
	    {
	    	static int loopcnt = 0;
	    	loopcnt++;
	    	if (loopcnt % 3 == 0)
	    	{
	    		/*keepalive every 15 seconds*/
	    		g_RemoteServ->send_auth_quest_msg();
	    	}
	    }
	}
    MUTEX_UNLOCK(m_remote_srv_lock);
}

static void _session_update_timer_callback(void* param1, void* param2,
                void* param3, void* param4)
{
	static uint64_t latest_total_data_req_cnt = 0;
	static uint64_t latest_total_data_req_byte = 0;

	static uint64_t latest_total_data_resp_cnt = 0;
	static uint64_t latest_total_data_resp_byte = 0;

	g_total_data_req_pps = (double)(g_total_data_req_cnt - latest_total_data_req_cnt)/SESSION_UPDATE_TIME;
	g_total_data_req_bps = (double)(g_total_data_req_byte - latest_total_data_req_byte) * 8 / SESSION_UPDATE_TIME;
	g_total_data_resp_pps = (double)(g_total_data_resp_cnt - latest_total_data_resp_cnt) / SESSION_UPDATE_TIME;
	g_total_data_resp_bps = (double)(g_total_data_resp_byte - latest_total_data_resp_byte) * 8 / SESSION_UPDATE_TIME;

	latest_total_data_req_cnt = g_total_data_req_cnt;
	latest_total_data_req_byte = g_total_data_req_byte;
	latest_total_data_resp_cnt = g_total_data_resp_cnt;
	latest_total_data_resp_byte = g_total_data_resp_byte;

	/*通知主窗口更新*/
    ::PostMessage(g_parentWnd, WM_UPDATE_PROXY_IP_LIST, NULL, NULL);
}

int proxy_init()
{
	g_parentWnd = AfxGetMainWnd()->m_hWnd;

	HMODULE module = GetModuleHandle(0);    
    GetModuleFileName(module, g_localModDir, MAX_PATH);
    char *ch = _tcsrchr(g_localModDir, _T('\\')); //查找最后一个\出现的位置，并返回\后面的字符（包括\）
    ch[1] = 0;//NULL  通过操作来操作szFilePath = 将szFilePath截断，截断最后一个\后面的字符（不包括\）

	loggger_init(g_localModDir, (char*)("GoProxy"), 1 * 1024, 6, false);

	if (FALSE == np_init())
	{
		MessageBox(NULL, _T("np init failed"), _T("Error"), MB_OK);

		_LOG_ERROR("netsock init failed.");
		loggger_exit();
		return -1;
	}

	g_syslogMgr = new CSyslogMgr();

	/*initialize config*/
	if(0 != proxy_cfg_init())
	{
		loggger_exit();
		return -1;
	}
	logger_set_level(g_log_level);	
	np_init_worker_thrds(g_thrd_cnt);

	if(0 != proc_comm_init())
	{
		MessageBox(NULL, _T("init proc comm server failed!"), _T("Error"), MB_OK);
		loggger_exit();
		return -1;
	}

	proxy_proc_mgr_init();

    if (0 != socks_mem_init(CLIENT_MEM_NODE_CNT))
    {
    	MessageBox(NULL, _T("socks mem init failed!"), _T("Error"), MB_OK);
        loggger_exit();
        return -1;
    }

    g_ConnMgr = CConnMgr::instance();
    
    if (g_is_start)
    {
    	proxy_proc_mgr_start();

    	proxy_cfg_t* cfginfo = proxy_cfg_get();
	    g_LocalServ = new CLocalServer(cfginfo->local_port);
	    if(0 != g_LocalServ->init())
	    {
	    	MessageBox(NULL, _T("local server init failed!"), _T("Error"), MB_OK);
	        loggger_exit();
	        return -1;
	    }
	}

	MUTEX_SETUP(m_remote_srv_lock);
	_remote_chk_timer_callback(NULL,NULL,NULL,NULL);

	/*start check timer*/
    np_add_time_job(_remote_chk_timer_callback, NULL, NULL, NULL, NULL, 5, FALSE);
	/*start update timer*/
    np_add_time_job(_session_update_timer_callback, NULL, NULL, NULL, NULL, SESSION_UPDATE_TIME, FALSE);

	inject_init();

	/*start check timer*/
	np_start();

	_LOG_INFO("start proxy success");
	g_syslogMgr->add_syslog("start proxy success");
	return 0;
}

void proxy_free()
{
	if (g_is_start)
	{
		proxy_proc_mgr_stop();
	}

	proxy_proc_mgr_free();
	np_let_stop();
	np_wait_stop();
	loggger_exit();

	delete g_syslogMgr;
	g_syslogMgr = NULL;
	return;
}

int proxy_reset()
{
	/*stop proxy*/
	if (g_is_start)
	{
		proxy_proc_mgr_stop();
	}

	g_LocalServ->free();
	g_LocalServ = NULL;

	MUTEX_LOCK(m_remote_srv_lock);
	if (g_RemoteServ != NULL)
	{
		g_RemoteServ->free();
	}
	MUTEX_UNLOCK(m_remote_srv_lock);

	np_del_time_job(_remote_chk_timer_callback, NULL);
	np_del_time_job(_session_update_timer_callback, NULL);

	proxy_proc_mgr_free();
	np_let_stop();
	np_wait_stop();
	_LOG_INFO("clear proxy env for reset");
	g_syslogMgr->add_syslog("clear proxy env for reset");

	/***********************************************/
	if (FALSE == np_init())
	{
		MessageBox(NULL, _T("np init failed"), _T("Error"), MB_OK);
		_LOG_ERROR("netsock init failed.");
		return -1;
	}
	np_init_worker_thrds(g_thrd_cnt);

	if(0 != proc_comm_init())
	{
		MessageBox(NULL, _T("init proc comm server failed!"), _T("Error"), MB_OK);
		return -1;
	}

	proxy_proc_mgr_init();

	if (g_is_start)
	{
		proxy_proc_mgr_start();

		proxy_cfg_t* cfginfo = proxy_cfg_get();
	    g_LocalServ = new CLocalServer(cfginfo->local_port);
	    if(0 != g_LocalServ->init())
	    {
	    	MessageBox(NULL, _T("local server init failed!"), _T("Error"), MB_OK);
	        return -1;
	    }
	}   

    _remote_chk_timer_callback(NULL,NULL,NULL,NULL);

	/*start check timer*/
    np_add_time_job(_remote_chk_timer_callback, NULL, NULL, NULL, NULL, 5, FALSE);
	/*start update timer*/
    np_add_time_job(_session_update_timer_callback, NULL, NULL, NULL, NULL, SESSION_UPDATE_TIME, FALSE);

	/*start check timer*/
	np_start();	

	_LOG_INFO("setup proxy env for reset");
	g_syslogMgr->add_syslog("setup proxy env for reset");
	return 0;
}
