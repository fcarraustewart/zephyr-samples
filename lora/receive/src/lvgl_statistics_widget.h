
/* ===================== lvgl_statistics_widget.h ===================== */

#ifndef LVGL_STATISTICS_WIDGET_H
#define LVGL_STATISTICS_WIDGET_H

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <stdint.h>

#define STAT_WIDGET_MSGQ_ITEMS 16
#define STAT_WIDGET_MSG_PAYLOAD 64

/* Message structure placed into the widget's k_msgq */
struct widget_msg {
    uint8_t opcode;
    uint8_t length; /* valid payload length */
    uint8_t data[STAT_WIDGET_MSG_PAYLOAD];
};

/* Opcodes for statistics widget */
enum stat_opcode {
    STAT_OPCODE_BLE = 1,   /* data: uint32_t packets */
    STAT_OPCODE_LORA = 2,  /* data: uint32_t packets */
    STAT_OPCODE_WIFI = 3,  /* data: uint32_t packets */
    STAT_OPCODE_RESET = 0xF0
};

/* Public API */
int stat_widget_init(lv_obj_t *parent);
struct k_msgq *stat_widget_msgq(void); /* return pointer to internal msgq for outsiders to put messages */

#endif /* LVGL_STATISTICS_WIDGET_H */
