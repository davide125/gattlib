/*
 *
 *  GattLib - GATT Library
 *
 *  Copyright (C) 2016-2021  Olivier Martin <olivier@labapart.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#ifdef GATTLIB_LOG_BACKEND_SYSLOG
#include <syslog.h>
#endif

#include "gattlib.h"

#define MIN(a,b)	((a)<(b)?(a):(b))

#define NUS_CHARACTERISTIC_TX_UUID	"6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_CHARACTERISTIC_RX_UUID	"6e400003-b5a3-f393-e0a9-e50e24dcca9e"

gattlib_connection_t* m_connection;

void notification_cb(const uuid_t* uuid, const uint8_t* data, size_t data_length, void* user_data) {
	int i;
	for(i = 0; i < data_length; i++) {
		printf("%c", data[i]);
	}
    fflush(stdout);
}

static void usage(char *argv[]) {
	printf("%s <device_address>\n", argv[0]);
}

void int_handler(int dummy) {
	gattlib_disconnect(m_connection, false /* wait_disconnection */);
	exit(0);
}

int main(int argc, char *argv[]) {
	char input[256];
	char* input_ptr;
	int i, ret, total_length, length = 0;
	uuid_t nus_characteristic_tx_uuid;
	uuid_t nus_characteristic_rx_uuid;

	if (argc != 2) {
		usage(argv);
		return 1;
	}

#ifdef GATTLIB_LOG_BACKEND_SYSLOG
	openlog("gattlib_nordic_uart", LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER);
	setlogmask(LOG_UPTO(LOG_INFO));
#endif

	m_connection = gattlib_connect(NULL, argv[1],
				       GATTLIB_CONNECTION_OPTIONS_LEGACY_BDADDR_LE_RANDOM |
				       GATTLIB_CONNECTION_OPTIONS_LEGACY_BT_SEC_LOW);
	if (m_connection == NULL) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to connect to the bluetooth device.");
		return 1;
	}

	// Convert characteristics to their respective UUIDs
	ret = gattlib_string_to_uuid(NUS_CHARACTERISTIC_TX_UUID, strlen(NUS_CHARACTERISTIC_TX_UUID) + 1, &nus_characteristic_tx_uuid);
	if (ret) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to convert characteristic TX to UUID.");
		return 1;
	}
	ret = gattlib_string_to_uuid(NUS_CHARACTERISTIC_RX_UUID, strlen(NUS_CHARACTERISTIC_RX_UUID) + 1, &nus_characteristic_rx_uuid);
	if (ret) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to convert characteristic RX to UUID.");
		return 1;
	}

	// Look for handle for NUS_CHARACTERISTIC_TX_UUID
	gattlib_characteristic_t* characteristics;
	int characteristic_count;
	ret = gattlib_discover_char(m_connection, &characteristics, &characteristic_count);
	if (ret) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to discover characteristic.");
		return 1;
	}

	uint16_t tx_handle = 0, rx_handle = 0;
	for (i = 0; i < characteristic_count; i++) {
		if (gattlib_uuid_cmp(&characteristics[i].uuid, &nus_characteristic_tx_uuid) == 0) {
			tx_handle = characteristics[i].value_handle;
		} else if (gattlib_uuid_cmp(&characteristics[i].uuid, &nus_characteristic_rx_uuid) == 0) {
			rx_handle = characteristics[i].value_handle;
		}
	}
	if (tx_handle == 0) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to find NUS TX characteristic.");
		return 1;
	} else if (rx_handle == 0) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to find NUS RX characteristic.");
		return 1;
	}
	free(characteristics);

	// Register notification handler
	gattlib_register_notification(m_connection, notification_cb, NULL);

	ret = gattlib_notification_start(m_connection, &nus_characteristic_rx_uuid);
	if (ret) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to start notification.");
		return 2;
	}

	// Register handler to catch Ctrl+C
	signal(SIGINT, int_handler);

	while(1) {
		fgets(input, sizeof(input), stdin);

		// NUS TX can only receive 20 bytes at a time
		input_ptr = input;
		for (total_length = strlen(input) + 1; total_length > 0; total_length -= length) {
			length = MIN(total_length, 20);
			ret = gattlib_write_without_response_char_by_handle(m_connection, tx_handle, input_ptr, length);
			if (ret) {
				GATTLIB_LOG(GATTLIB_ERROR, "Fail to send data to NUS TX characteristic.");
				return 1;
			}
			input_ptr += length;
		}
	}

	return 0;
}
