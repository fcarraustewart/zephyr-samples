/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdio.h>

#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(logging_blog, LOG_LEVEL_DBG);
#include <static_any.hpp>
#define STACK_SIZE_TEST 1024
#include <zephyr/logging/log.h>
#include <zephyr/sys/heap_listener.h>
#include <zephyr/zbus/zbus.h>

extern struct sys_heap _system_heap;
static size_t total_allocated;


// gdbstub test debugging instructions
// https://docs.zephyrproject.org/latest/services/debugging/gdbstub.html
// west build -p always -b qemu_cortex_m3 tests/subsys/debug/gdbstub -- '-DCONFIG_QEMU_EXTRA_FLAGS="-serial tcp:localhost:5678,server"'
//     add -- Found devicetree overlay: /Users/fel_c/zephyrproject/zephyr/tests/subsys/debug/gdbstub/boards/qemu_cortex_m3.overlay

/*		DEBUGGING QEMU
west build --build-dir /Users/fel_c/zephyrproject/zephyr/samples/hello_world/build /Users/fel_c/zephyrproject/zephyr/samples/hello_world --pristine auto --board qemu_cortex_m3/ti_lm3s6965 -- -DNCS_TOOLCHAIN_VERSION="NONE" -DBOARD_ROOT="/Users/fel_c/zephyrproject/zephyr/samples/hello_world"
*/
#define STACKSIZE 512

struct acc_msg {
	int x;
	int y;
	int z;
};
struct controls_msg {
	char op;
	char payload[63];
};


static void listener_callback_example(const struct zbus_channel *chan)
{
	const struct acc_msg *acc = (struct acc_msg *)zbus_chan_const_msg(chan);

	LOG_INF("From listener foo_lis -> Acc x=%d, y=%d, z=%d", acc->x, acc->y, acc->z);
}

ZBUS_LISTENER_DEFINE(foo_lis, listener_callback_example);


ZBUS_CHAN_DEFINE(acc_data_chan,  /* Name */
		 struct acc_msg, /* Message type */

		 NULL, /* Validator */
		 NULL, /* User data */
		 ZBUS_OBSERVERS(bar_sub1, bar_msg_sub1, bar_msg_sub2, bar_msg_sub3, bar_msg_sub4, foo_lis), /* observers */
		 ZBUS_MSG_INIT(.x = 0, .y = 0, .z = 0)  /* Initial value */
);

ZBUS_CHAN_DEFINE(controls_chan,  /* Name */
	struct controls_msg, /* Message type */

	NULL, /* Validator */
	NULL, /* User data */
	ZBUS_OBSERVERS(bar_sub1), /* observers */
	ZBUS_MSG_INIT(.op = 0, .payload = {0})  /* Initial value */
);


ZBUS_MSG_SUBSCRIBER_DEFINE(bar_msg_sub1);
ZBUS_MSG_SUBSCRIBER_DEFINE(bar_msg_sub2);
ZBUS_MSG_SUBSCRIBER_DEFINE(bar_msg_sub3);
ZBUS_MSG_SUBSCRIBER_DEFINE(bar_msg_sub4);
ZBUS_MSG_SUBSCRIBER_DEFINE(bar_msg_sub5);

ZBUS_SUBSCRIBER_DEFINE(bar_sub1, 4);
ZBUS_SUBSCRIBER_DEFINE(bar_sub2, 4);

static void msg_subscriber_task(void *sub)
{
	const struct zbus_channel *chan;

	struct acc_msg acc;

	const struct zbus_observer *subscriber =
	reinterpret_cast<const struct zbus_observer*>(sub);

	while (!zbus_sub_wait_msg(subscriber, &chan, &acc, K_FOREVER)) {
		if (&acc_data_chan != chan) {
			LOG_ERR("Wrong channel %p!", chan);

			continue;
		}
		LOG_INF("From msg subscriber %s -> Acc x=%d, y=%d, z=%d", zbus_obs_name(subscriber),
			acc.x, acc.y, acc.z);
	}
}

K_THREAD_DEFINE(subscriber_task_id1, STACK_SIZE_TEST, msg_subscriber_task, &bar_msg_sub1,
		NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(subscriber_task_id2, STACK_SIZE_TEST, msg_subscriber_task, &bar_msg_sub2,
		NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(subscriber_task_id3, STACK_SIZE_TEST, msg_subscriber_task, &bar_msg_sub3,
		NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(subscriber_task_id4, STACK_SIZE_TEST, msg_subscriber_task, &bar_msg_sub4,
		NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(subscriber_task_id5, STACK_SIZE_TEST, msg_subscriber_task, &bar_msg_sub5,
		NULL, NULL, 3, 0, 0);
static void subscriber_task(void *sub)
{
	const struct zbus_channel *chan;

	struct acc_msg acc;
	struct controls_msg control;

	const struct zbus_observer *subscriber =
	reinterpret_cast<const struct zbus_observer*>(sub);

	while (!zbus_sub_wait(subscriber, &chan, K_FOREVER)) {
		if (&acc_data_chan == chan) {
			LOG_ERR("Wrong channel %p!", chan);

			zbus_chan_read(chan, &acc, K_MSEC(250));

			LOG_INF("From subscriber %s -> Acc x=%d, y=%d, z=%d", zbus_obs_name(subscriber),
				acc.x, acc.y, acc.z);

			continue;
		}
		if (&controls_chan == chan) {
			LOG_ERR("Wrong channel %p!", chan);

			zbus_chan_read(chan, &control, K_MSEC(250));

			LOG_INF("From subscriber %s -> Control opc=%d", zbus_obs_name(subscriber),
				control.op);

			LOG_HEXDUMP_INF(control.payload, 63, "Control payload");	
				
			continue;
		}
	}
}

K_THREAD_DEFINE(subscriber_task_id17, CONFIG_MAIN_STACK_SIZE, subscriber_task, &bar_sub1, NULL,
	NULL, 2, 0, 0);
K_THREAD_DEFINE(subscriber_task_id18, CONFIG_MAIN_STACK_SIZE, subscriber_task, &bar_sub2, NULL,
	NULL, 4, 0, 0);

ZBUS_CHAN_ADD_OBS(acc_data_chan, bar_sub2, 3);
ZBUS_CHAN_ADD_OBS(acc_data_chan, bar_msg_sub5, 3);

// Kalman Kinematic Arbiter:
// python /Users/fel_c/mentalista/kinematic_arbiter/src/single_dof_demo/demo.py
#include <System.hpp>
#include <ActiveObject.hpp>
#include <Services/LoRa.hpp>


extern "C" {
	
	static struct acc_msg acc = {.x = 1, .y = 10, .z = 100};
	static struct controls_msg ctrl = {.op = 1, .payload = {0}};

	void on_heap_alloc(uintptr_t heap_id, void *mem, size_t bytes)
	{
		total_allocated += bytes;
		LOG_INF(" AL Memory allocated %u bytes. Total allocated %u bytes", (unsigned int)bytes,
			(unsigned int)total_allocated);
	}
	
	void on_heap_free(uintptr_t heap_id, void *mem, size_t bytes)
	{
		total_allocated -= bytes;
		LOG_INF(" FR Memory freed %u bytes. Total allocated %u bytes", (unsigned int)bytes,
			(unsigned int)total_allocated);
	}
	
	#if defined(CONFIG_ZBUS_MSG_SUBSCRIBER_BUF_ALLOC_DYNAMIC)
	
	// HEAP_LISTENER_ALLOC_DEFINE(my_heap_listener_alloc, HEAP_ID_FROM_POINTER(&_system_heap),
	// 			   on_heap_alloc);
	
	// HEAP_LISTENER_FREE_DEFINE(my_heap_listener_free, HEAP_ID_FROM_POINTER(&_system_heap), on_heap_free);
	
	#endif /* CONFIG_ZBUS_MSG_SUBSCRIBER_BUF_ALLOC_DYNAMIC */
	#define MAX_INT_K 1024
	static int count = 0;
	
	int main(void)
	{

		total_allocated = 0;
	#if defined(CONFIG_ZBUS_MSG_SUBSCRIBER_NET_BUF_POOL_ISOLATION)
		// zbus_chan_set_msg_sub_pool(&acc_data_chan, &isolated_pool);
	#endif
	
	#if defined(CONFIG_ZBUS_MSG_SUBSCRIBER_BUF_ALLOC_DYNAMIC)
	
		// heap_listener_register(&my_heap_listener_alloc);
		// heap_listener_register(&my_heap_listener_free);
	
	#endif /* CONFIG_ZBUS_MSG_SUBSCRIBER_BUF_ALLOC_DYNAMIC */

		LOG_INF("%s():enter", __func__);

		while(1)
		{
			k_msleep(100);

			LOG_INF("----> Publishing to %s channel", zbus_chan_name(&acc_data_chan));
			zbus_chan_pub(&acc_data_chan, &acc, K_NO_WAIT);
			zbus_chan_pub(&controls_chan, &ctrl, K_NO_WAIT);
			acc.x += 1;
			acc.y += 10;
			acc.z += 100;
			for(int i = 0; i<63; i++) {
				count++;
				ctrl.payload[i] = count;
			}
			Service::LoRa::Send((uint8_t*)&count);
			// __ASSERT(var, "False forced assert in function: %s()", __FUNCTION__, 0);
		}
	
		return 0;
	}
	

}