/*
 * app_timekeeper.c
 *
 *  Created on: May 18, 2026
 *      Author: maty
 */


#include "main.h" // change on integration
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdbool.h>

#include "svc_tk.h"
#include "tk_defines.h"
#include "server_tk.h" //correct the path in integration (also applies to things lower)

#include <csp_pld_txn.h>
#include <pld_tk_plugin.h>
#include "csp/csp_rtable.h"

/*
 *
 * TYPEDEFS
 *
 */

typedef enum {
	ST_IDLE,
	ST_SYNC_ITERATE_SEND,
	ST_SYNC_DONE,
	ST_SYNC_ERR,
} st_timekeeper_t;

typedef struct {
	st_timekeeper_t state;
	err_timekeeper_t err;

	tk_sync_mode_t sync_mode;
	uint64_t frequency;
	uint64_t last_sync;
	uint8_t last_failed_node;

	uint8_t master_node;
} timekeeper_data_t;


/*
 *
 * TK GLOBAL DEFINES
 *
 */

osThreadId_t timekeeperTaskHandle;
const osThreadAttr_t timekeeperTask_attributes = {
    .name = "timekeeperTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 1024 * 4
};

static osSemaphoreId_t tk_start_sem;
static osSemaphoreId_t tk_wait_finish_sem;
static osSemaphoreId_t tk_sync_wait_response_sem;

static timekeeper_data_t tk_data = {0};

#define MAX_SYNC_DONE_WAIT 10000
#define DEF_SYNC_FREQUENCY 108000 // default frequency between syncing - 30mins

/*
 *
 * PRIVATE FUNCTIONS
 *
 */

// function to send the set time request
static int tk_sync_slaves_req(uint8_t node, uint64_t unix_time, struct TK_plugin_TK_SYNC_SLAVES_RSP* rsp){
	if (node == csp_get_address()){
		return CSP_ERR_INVAL;
	}


	// request build
	struct TK_plugin_TK_SYNC_SLAVES_REQ_pack {
		struct TK_plugin_TK_SYNC_SLAVES_REQ cmd;
		} pack = {
			.cmd = {
				.pld_id = TK_PLUGIN_TK_SYNC_SLAVES_REQ_ID,
				.time = unix_time,
				.node = csp_get_address(),
		}
	};

	uint8_t req_buff[TK_PLUGIN_TK_SYNC_SLAVES_REQ_BIN_SIZE];
	uint8_t rsp_buff[TK_PLUGIN_TK_SYNC_SLAVES_RSP_BIN_SIZE];

	pld_error_t ret = TK_plugin_TK_SYNC_SLAVES_REQ_build_s(&pack.cmd, req_buff, TK_PLUGIN_TK_SYNC_SLAVES_REQ_BIN_SIZE);

	if(ret != PLD_OK) {
		return CSP_ERR_INVAL;
	}

	int size = csp_transaction(
			CSP_PRIO_NORM,
			node,
			TIMEKEEPER_PORT,
			1000,
			req_buff,
			TK_PLUGIN_TK_SYNC_SLAVES_REQ_BIN_SIZE,
			rsp_buff,
			TK_PLUGIN_TK_SYNC_SLAVES_RSP_BIN_SIZE
	);

	if (size <= 0) {
        return CSP_ERR_TIMEDOUT;
	}

	ret = TK_plugin_TK_SYNC_SLAVES_RSP_parse_s(rsp_buff, TK_PLUGIN_TK_SYNC_SLAVES_RSP_BIN_SIZE, rsp);

	if(ret != PLD_OK) {
		return CSP_ERR_INVAL;
	}

	return CSP_ERR_NONE;
}

// A csp iterator to iterate over the routing table (foreach)
bool timekeeper_csp_iterate(void * ctx, uint8_t address, uint8_t mask, const csp_route_t * route){
	(void)ctx;(void)mask;(void)route;

	csp_timestamp_t time = {0};
	csp_clock_get_time(&time);

	struct TK_plugin_TK_SYNC_SLAVES_RSP rsp = {0};

	int res = tk_sync_slaves_req(address, time.tv_sec, &rsp);

	if (res != CSP_ERR_NONE){
		tk_data.err = TK_ERR;
		tk_data.last_failed_node = address;
	}

	return true;
}

static void timekeeper_main(){
	while (1){
		switch (tk_data.state){
			case ST_IDLE:{
				osSemaphoreAcquire(tk_start_sem, tk_data.frequency);
				tk_data.err = TK_NOERR;

				if (tk_data.master_node == 0 && tk_data.sync_mode != TK_MODE_STOP && tk_data.sync_mode != TK_MODE_SLAVE){
					tk_data.state = ST_SYNC_ITERATE_SEND;
				}
			} break;
			case ST_SYNC_ITERATE_SEND:{
				csp_rtable_iterate(timekeeper_csp_iterate, NULL);

				tk_data.state = ST_SYNC_DONE;
			} break;
			case ST_SYNC_DONE:{
				if (tk_data.sync_mode == TK_MODE_ONCE){
					tk_data.sync_mode = TK_MODE_STOP;
				}

				osSemaphoreRelease(tk_wait_finish_sem);
				tk_data.state = ST_IDLE;
			} break;

			default:{
				tk_data.err = TK_ERR;
				break;
			}
		}
	}
}

static void timekeeper_init(){
	tk_data.state = ST_IDLE;
	tk_data.err = TK_NOERR;
	tk_data.frequency = DEF_SYNC_FREQUENCY;
	tk_data.sync_mode = TK_MODE_STOP;
	tk_data.last_sync = 0;
	tk_data.last_failed_node = 0;
	tk_data.master_node = 0;

	tk_start_sem = osSemaphoreNew(1, 0, NULL);
	tk_wait_finish_sem = osSemaphoreNew(1, 0, NULL);
	tk_sync_wait_response_sem = osSemaphoreNew(1, 0, NULL);

	if (tk_start_sem == NULL || tk_wait_finish_sem == NULL || tk_sync_wait_response_sem == NULL){
		tk_data.err = TK_ERR;

		return;
	}

	timekeeper_main();
}

/*
 *
 * PUBLIC APP FUNCTIONS
 *
 */

int tk_app_get_time(csp_timestamp_t * time){
	csp_clock_get_time(time);

	return TK_NOERR;
}

int tk_app_set_time(const csp_timestamp_t * time){
	int status = csp_clock_set_time(time);

	return (status == CSP_ERR_NONE) ? TK_NOERR : TK_ERR;
}

void tk_app_sync_time(long frequency, uint8_t flags, struct TK_plugin_TK_SYNC_TIME_RSP* rsp){
	if (frequency > 0){
		tk_data.frequency = frequency;
	}

	if (flags & STATUS_FLAG_MASK && !(flags & STOP_FLAG_MASK || flags & START_FLAG_MASK || flags & ONCE_FLAG_MASK)){
		rsp->error = tk_data.err;
		rsp->last_sync = tk_data.last_sync;
		rsp->master_node = tk_data.master_node;
		rsp->mode = tk_data.sync_mode;
		rsp->ret_val = 0;

		return;
	}

	if (flags & STOP_FLAG_MASK){
		tk_data.sync_mode = TK_MODE_STOP;
	}

	if (flags & START_FLAG_MASK){
		tk_data.sync_mode = TK_MODE_CONTINUOUS;
	}

	if (flags & ONCE_FLAG_MASK){
		tk_data.sync_mode = TK_MODE_ONCE;
	}

	tk_data.master_node = 0;

	while (osSemaphoreAcquire(tk_wait_finish_sem, 0) == osOK) {
	    // Do nothing, just clear it out
	}

	osSemaphoreRelease(tk_start_sem);

	if (flags & WAIT_RES_FLAG_MASK){
		osSemaphoreAcquire(tk_wait_finish_sem, MAX_SYNC_DONE_WAIT); // if we are supposed to wait for result, wait
	}

	csp_timestamp_t now = {0};
	csp_clock_get_time(&now);

	tk_data.last_sync = now.tv_sec;

	rsp->error = tk_data.err;
	rsp->last_sync = tk_data.last_sync;
	rsp->master_node = tk_data.master_node;
	rsp->mode = tk_data.sync_mode;
	rsp->ret_val = 0;

	return;
}

void tk_app_make_slave(uint8_t master_node){
	tk_data.master_node = master_node;

	csp_timestamp_t now = {0};
	csp_clock_get_time(&now);

	tk_data.last_sync = now.tv_sec;

	tk_data.sync_mode = TK_MODE_SLAVE;
}

/*
void app_init_timekeeper(){
	timekeeperTaskHandle = osThreadNew(timekeeper_init, NULL, &timekeeperTask_attributes);
}
*/


/*

	SALLY ABSTRACTION !! EXPERIMENTAL !! im not convinced i know how it works

*/

service_thread_t top_thread = {
	.attr = timekeeperTask_attributes,
	.arg = NULL,
	.func = timekeeper_init,
	.handle = timekeeperTaskHandle
};

service_desc_t service_per = {
  .port = TK_SERVICE_PORT, // TODO on integration
  .th_top = top_thread,
  .th_spawned = SERVICE_THREAD_NONE,
  .log_flag_index = { SERVICE_LOGGING_LEVEL_UNUSED, SERVICE_LOGGING_LEVEL_UNUSED },
  .cb_log_setup = { NULL, NULL },
  .cb_conn_accepted = NULL,
  .cb_packet_read = server_process_timekeeper_packet,
  .cb_init = NULL,
  .cb_init_args = NULL,
  .cb_reg = NULL, //persist_register_callback,
  .cbs = NULL, //{ wdg_feed, NULL, readPersist_mem, writePersist_mem },
};