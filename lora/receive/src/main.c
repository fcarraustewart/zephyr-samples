/*
 * Copyright (c) 2019 Manivannan Sadhasivam
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <lvgl_input_device.h>

static uint32_t count;

#include "lvgl_statistics_widget.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
	     "No default LoRa radio specified in DT");

#define MAX_DATA_LEN 255

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lora_receive);

void lora_receive_cb(const struct device *dev, uint8_t *data, uint16_t size,
		     int16_t rssi, int8_t snr, void *user_data)
{
	static int cnt;

	ARG_UNUSED(dev);
	ARG_UNUSED(size);
	ARG_UNUSED(user_data);

	LOG_INF("LoRa RX RSSI: %d dBm, SNR: %d dB", rssi, snr);
	LOG_HEXDUMP_INF(data, size, "LoRa RX payload");

	/* Stop receiving after 1000000 packets */
	if (++cnt == 1000000) {
		LOG_INF("Stopping packet receptions");
		lora_recv_async(dev, NULL, NULL);
	}

	static struct widget_msg m;
	m.opcode = STAT_OPCODE_LORA; m.length = 4; memcpy(m.data, &cnt, 4);
	k_msgq_put(stat_widget_msgq(), &m, K_NO_WAIT);
	
}

int main(void)
{
	const struct device *display_dev;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return 0;
	}

    lv_obj_t *scr = lv_scr_act();

    /* place stats at top, sensors at bottom */
    lv_obj_t *top_cont = lv_obj_create(scr);
    lv_obj_set_size(top_cont, 128, 64);
    lv_obj_align(top_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_cont, LV_OPA_TRANSP, LV_PART_MAIN);


    stat_widget_init(top_cont);

    display_blanking_off(display_dev);

	const struct device *const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
	struct lora_modem_config config;
	int ret, len;
	uint8_t data[MAX_DATA_LEN] = {0};
	int16_t rssi;
	int8_t snr;

	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s Device not ready", lora_dev->name);
		return 0;
	}

	config.frequency = 433000000;
	config.bandwidth = BW_125_KHZ;
	config.datarate = SF_10;
	config.preamble_len = 8;
	config.coding_rate = CR_4_5;
	config.iq_inverted = false;
	config.public_network = false;
	config.tx_power = 14;
	config.tx = false;

	ret = lora_config(lora_dev, &config);
	if (ret < 0) {
		LOG_ERR("LoRa config failed");
		return 0;
	}

	/* Enable asynchronous reception */
	LOG_INF("Asynchronous reception");
	lora_recv_async(lora_dev, lora_receive_cb, NULL);

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(10));
    }
	
	return 0;
}
