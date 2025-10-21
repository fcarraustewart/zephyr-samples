/**
 * @file main.c
 * @author Jeronimo Agullo (jeronimoagullo97@gmail.com)
 * @brief 
 * @version 1.0
 * @date 2024-04-08
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gnss.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zephyr_gnss_sample, LOG_LEVEL_DBG);


static struct gnss_data mGNSSData = (struct gnss_data){0};;
static void gnss_data_cb(const struct device *dev, const struct gnss_data *data)
{
	memcpy(&mGNSSData, data, sizeof(struct gnss_data));
	if (data->info.fix_status != GNSS_FIX_STATUS_NO_FIX) {
		LOG_INF("GNSS Fix Acquired!\n");
		LOG_INF("\tdate: %02d:%02d:%02d:%03d %02d-%02d-%04d", 
				data->utc.hour, data->utc.minute, data->utc.millisecond/1000, data->utc.millisecond%1000, 
				data->utc.month_day, data->utc.month, data->utc.century_year + 2000);
		LOG_INF("\t%s location (%lld, %lld) with %d tracked satellites", 
				dev->name, data->nav_data.latitude, data->nav_data.longitude, data->info.satellites_cnt);
		LOG_INF("\taltitude:\t %d m, \tspeed:\t %d km/h, \tbearing:\t %d deg", 
				data->nav_data.altitude/1000, data->nav_data.speed*36/1000, data->nav_data.bearing/1000);
		LOG_INF("\thdop:\t %d, \tgeoid_separation:\t %d m", 
				data->info.hdop, data->info.geoid_separation);
	} else {
		LOG_INF("no fix. Searching sattelites...");
	}
}

GNSS_DATA_CALLBACK_DEFINE(DEVICE_DT_GET(DT_ALIAS(gnss)), gnss_data_cb);

#if CONFIG_GNSS_SATELLITES
static void gnss_satellites_cb(const struct device *dev, const struct gnss_satellite *satellites,
			       uint16_t size)
{
	LOG_INF("%s reported %u satellites!", dev->name, size);
}
#endif

GNSS_SATELLITES_CALLBACK_DEFINE(DEVICE_DT_GET(DT_ALIAS(gnss)), gnss_satellites_cb);

int main(void)
{
	LOG_INF("Starting Zephyr GNSS Sample");

	return 0;
}
