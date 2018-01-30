/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include "net/wlan/wlan.h"
#include "net/wlan/wlan_defs.h"
#include "lwip/netif.h"

#include "cmd_util.h"
#include "common/framework/net_ctrl.h"

#include "smartlink/sc_assistant.h"
#include "smartlink/airkiss/wlan_airkiss.h"

#define AK_TIME_OUT_MS 120000

static int ak_key_used;
static char *airkiss_key = "1234567812345678";

static OS_Thread_t g_thread;
#define THREAD_STACK_SIZE       (1 * 1024)

static void ak_task(void *arg)
{
	wlan_airkiss_status_t ak_status;
	wlan_airkiss_result_t ak_result;
	sc_assistant_fun_t sca_fun;

	memset(&ak_result, 0, sizeof(wlan_airkiss_result_t));

	sc_assistant_get_fun(&sca_fun);
	sc_assistant_init(g_wlan_netif, &sca_fun, AK_TIME_OUT_MS);

	ak_status = wlan_airkiss_start(g_wlan_netif, ak_key_used ? airkiss_key : NULL);
	if (ak_status != WLAN_AIRKISS_SUCCESS) {
		CMD_DBG("airkiss start fiald!\n");
		goto out;
	}

	CMD_DBG("%s getting ssid and psk...\n", __func__);

	if (wlan_airkiss_wait(AK_TIME_OUT_MS) == WLAN_AIRKISS_TIMEOUT) {
		goto out;
	}
	CMD_DBG("%s get ssid and psk finished\n", __func__);

	if (wlan_airkiss_get_status() == AIRKISS_STATUS_COMPLETE) {
		if (!wlan_airkiss_connect_ack(g_wlan_netif, AK_TIME_OUT_MS, &ak_result)) {
			CMD_DBG("ssid:%s psk:%s random:%d\n", (char *)ak_result.ssid,
			        (char *)ak_result.passphrase, ak_result.random_num);
		}
	}

out:
	wlan_airkiss_stop();
	sc_assistant_deinit(g_wlan_netif);
	OS_ThreadDelete(&g_thread);
}

static int cmd_airkiss_create(void)
{
	if (OS_ThreadCreate(&g_thread,
	                    "ak_thread",
	                    ak_task,
	                    NULL,
	                    OS_THREAD_PRIO_APP,
	                    THREAD_STACK_SIZE) != OS_OK) {
		CMD_ERR("create ak thread failed\n");
		return -1;
	}
	return 0;
}

static int cmd_airkiss_stop(void)
{
	return wlan_airkiss_stop();
}

enum cmd_status cmd_airkiss_exec(char *cmd)
{
	int ret = 0;

	if (g_wlan_netif == NULL) {
		return CMD_STATUS_FAIL;
	}

	if (cmd_strcmp(cmd, "set_key") == 0) {
		ak_key_used = 1;
		CMD_DBG("Airkiss set key : %s\n", airkiss_key);
	} else if (cmd_strcmp(cmd, "start") == 0) {
		if (OS_ThreadIsValid(&g_thread)) {
			CMD_ERR("Airkiss is already start\n");
			ret = -1;
		} else {
			ret = cmd_airkiss_create();
		}
	} else if (cmd_strcmp(cmd, "stop") == 0) {
		ret = cmd_airkiss_stop();
		ak_key_used = 0;
	} else {
		CMD_ERR("invalid argument '%s'\n", cmd);
		return CMD_STATUS_INVALID_ARG;
	}

	return (ret == 0 ? CMD_STATUS_OK : CMD_STATUS_FAIL);
}
