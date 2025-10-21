/*
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ADVERTISE_SMP_SERVICE_H
#define ADVERTISE_SMP_SERVICE_H

#include <stdint.h>
#include <zephyr/bluetooth/bluetooth.h>

void start_smp_bluetooth_adverts(void);
void connected_smp(struct bt_conn *conn, uint8_t err);
void on_conn_recycled_smp(void);
void bt_ready_smp(int err);

#endif /* ADVERTISE_SMP_SERVICE_H */