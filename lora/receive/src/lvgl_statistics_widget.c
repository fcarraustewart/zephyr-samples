/*
 * lvgl_statistics_widget.h/.c and lvgl_sensor_widget.h/.c
 * Compact, KISS C modules for Zephyr + LVGL
 * - Message API: each msg placed on module's k_msgq is struct widget_msg { uint8_t opcode; uint8_t length; uint8_t data[64]; }
 * - Uses Zephyr kernel primitives (k_msgq, k_thread, k_mutex, k_timer) and LVGL objects (chart, labels, containers)
 * - Safe-ish: GUI updates are performed from LVGL timers (lv_timer_cb), message thread updates protected by mutex.
 * - Designed to be included in a Zephyr LVGL app (see usage at bottom)
 *
 * Files contained in this single document:
 *  - lvgl_statistics_widget.h
 *  - lvgl_statistics_widget.c
 *  - lvgl_sensor_widget.h
 *  - lvgl_sensor_widget.c
 *
 * Read the comments in each file for API and opcodes.
 */

/* ===================== lvgl_statistics_widget.c ===================== */

#include "lvgl_statistics_widget.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stat_widget, LOG_LEVEL_DBG);

/* Internal storage sizes */
#define STAT_HISTORY_LEN 60 /* seconds of history */

static struct k_msgq _msgq;
static char _msgq_buffer[STAT_WIDGET_MSGQ_ITEMS * sizeof(struct widget_msg)];
static struct k_thread _msgq_thread_data;
static K_THREAD_STACK_DEFINE(_msgq_stack, 2048);
static struct k_mutex _data_lock;

/* LVGL objects */
static lv_obj_t *chart;
static lv_chart_series_t *series_ble;
static lv_chart_series_t *series_lora;
static lv_chart_series_t *series_wifi;
static lv_obj_t *lbl_ble;
static lv_obj_t *lbl_lora;
static lv_obj_t *lbl_wifi;

/* running counters */
static uint32_t counters_ble;
static uint32_t counters_lora;
static uint32_t counters_wifi;

/* history arrays for plotting */
static uint32_t history_ble[STAT_HISTORY_LEN];
static uint32_t history_lora[STAT_HISTORY_LEN];
static uint32_t history_wifi[STAT_HISTORY_LEN];
static size_t history_idx;

/* forward */
static void msgq_thread(void *p1, void *p2, void *p3);
static void lv_tick_cb(lv_timer_t *t);

int stat_widget_init(lv_obj_t *parent)
{
    /* create msgq */
    k_msgq_init(&_msgq, _msgq_buffer, sizeof(struct widget_msg), STAT_WIDGET_MSGQ_ITEMS);
    k_mutex_init(&_data_lock);

    /* create chart */
    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 128, 64);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, STAT_HISTORY_LEN);
    lv_obj_align(chart, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 0, LV_PART_MAIN);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    series_ble = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    series_lora = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    series_wifi = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    lbl_ble = lv_label_create(parent);
    lv_obj_align(lbl_ble, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_label_set_text(lbl_ble, "BLE: 0/s");

    lbl_lora = lv_label_create(parent);
    lv_obj_align(lbl_lora, LV_ALIGN_TOP_RIGHT, -2, 18);
    lv_label_set_text(lbl_lora, "LoRa: 0/s");

    lbl_wifi = lv_label_create(parent);
    lv_obj_align(lbl_wifi, LV_ALIGN_TOP_RIGHT, -2, 34);
    lv_label_set_text(lbl_wifi, "WiFi: 0/s");

    /* zero history */
    for (size_t i = 0; i < STAT_HISTORY_LEN; ++i) {
        history_ble[i] = 0;
        history_lora[i] = 0;
        history_wifi[i] = 0;
    }
    history_idx = 0;

    /* start worker thread to consume msgq */
    k_thread_create(&_msgq_thread_data, _msgq_stack, K_THREAD_STACK_SIZEOF(_msgq_stack), msgq_thread,
                    NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

    /* start lv_timer for GUI updates (1s) */
    lv_timer_create(lv_tick_cb, 1000, NULL);

    return 0;
}

struct k_msgq *stat_widget_msgq(void)
{
    return &_msgq;
}

/* Worker thread: read messages and update counters */
static void msgq_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    struct widget_msg msg;

    while (1) {
        if (k_msgq_get(&_msgq, &msg, K_FOREVER) == 0) {
            k_mutex_lock(&_data_lock, K_FOREVER);
            switch (msg.opcode) {
            case STAT_OPCODE_BLE: {
                if (msg.length >= sizeof(uint32_t)) {
                    uint32_t v;
                    memcpy(&v, msg.data, sizeof(uint32_t));
                    counters_ble += v;
                }
                break;
            }
            case STAT_OPCODE_LORA: {
                if (msg.length >= sizeof(uint32_t)) {
                    uint32_t v;
                    memcpy(&v, msg.data, sizeof(uint32_t));
                    counters_lora += v;
                }
                break;
            }
            case STAT_OPCODE_WIFI: {
                if (msg.length >= sizeof(uint32_t)) {
                    uint32_t v;
                    memcpy(&v, msg.data, sizeof(uint32_t));
                    counters_wifi += v;
                }
                break;
            }
            case STAT_OPCODE_RESET:
                counters_ble = counters_lora = counters_wifi = 0;
                break;
            default:
                /* ignore */
                break;
            }
            k_mutex_unlock(&_data_lock);
        }
    }
}

/* LVGL timer: called once per second to snapshot counters and update chart */
static void lv_tick_cb(lv_timer_t *t)
{
    ARG_UNUSED(t);
    uint32_t b, l, w;
    k_mutex_lock(&_data_lock, K_FOREVER);
    b = counters_ble; l = counters_lora; w = counters_wifi;
    counters_ble = counters_lora = counters_wifi = 0; /* reset per-second counters */
    k_mutex_unlock(&_data_lock);

    /* push into history ring buffer */
    history_ble[history_idx] = b;
    history_lora[history_idx] = l;
    history_wifi[history_idx] = w;
    history_idx = (history_idx + 1) % STAT_HISTORY_LEN;

    /* update chart series: LVGL expects arrays in chronological order */
    // for (size_t i = 0; i < STAT_HISTORY_LEN; ++i) {
    //     size_t idx = (history_idx + i) % STAT_HISTORY_LEN;
    //     lv_chart_set_next_value(chart, series_ble, history_ble[idx]);
    //     lv_chart_set_next_value(chart, series_lora, history_lora[idx]);
    //     lv_chart_set_next_value(chart, series_wifi, history_wifi[idx]);
    // }
    lv_chart_set_next_value(chart, series_ble, b);
    lv_chart_set_next_value(chart, series_lora, l);
    lv_chart_set_next_value(chart, series_wifi, w);
    lv_chart_refresh(chart);

    /* update labels */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "BLE: %u/s", b);
    lv_label_set_text(lbl_ble, tmp);
    snprintf(tmp, sizeof(tmp), "LoRa: %u/s", l);
    lv_label_set_text(lbl_lora, tmp);
    snprintf(tmp, sizeof(tmp), "WiFi: %u/s", w);
    lv_label_set_text(lbl_wifi, tmp);
}
