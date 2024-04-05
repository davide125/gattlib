/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-or-later
 *
 * Copyright (c) 2021-2024, Olivier Martin <olivier@labapart.org>
 */

#include "gattlib_internal.h"

// Keep track of the allocated adapters to avoid an adapter to be freed twice.
// It could happen when using Python wrapper.
GSList *m_adapter_list;

bool gattlib_adapter_is_valid(gattlib_adapter_t* adapter) {
	bool is_valid;

	g_rec_mutex_lock(&m_gattlib_mutex);

	GSList *adapter_entry = g_slist_find(m_adapter_list, adapter);
	if (adapter_entry == NULL) {
		is_valid = false;
	} else {
		is_valid = true;
	}

	g_rec_mutex_unlock(&m_gattlib_mutex);
	return is_valid;
}

bool gattlib_adapter_is_scanning(gattlib_adapter_t* adapter) {
	bool is_scanning = false;

	g_rec_mutex_lock(&m_gattlib_mutex);

	GSList *adapter_entry = g_slist_find(m_adapter_list, adapter);
	if (adapter_entry == NULL) {
		goto EXIT;
	}

	is_scanning = adapter->backend.ble_scan.is_scanning;

EXIT:
	g_rec_mutex_unlock(&m_gattlib_mutex);
	return is_scanning;
}

struct _device_is_valid {
    gattlib_device_t* device;
    bool found;
};

static void _gattlib_device_is_valid(gpointer data, gpointer user_data) {
	gattlib_adapter_t* adapter = data;
    struct _device_is_valid* device_is_valid = user_data;

	GSList *device_entry = g_slist_find(adapter->devices, device_is_valid->device);
	if (device_entry != NULL) {
		device_is_valid->found = true;
	}
}

bool gattlib_device_is_valid(gattlib_device_t* device) {
    struct _device_is_valid device_is_valid = {
        .device = device,
        .found = false
    };

    g_rec_mutex_lock(&m_gattlib_mutex);
    g_slist_foreach(m_adapter_list, _gattlib_device_is_valid, &device_is_valid);
    g_rec_mutex_unlock(&m_gattlib_mutex);

    return device_is_valid.found;
}

struct _connection_is_connected {
	gattlib_connection_t* connection;
	bool is_connected;
};

static gint _is_device_connection(gconstpointer a, gconstpointer b) {
	const gattlib_device_t* device = a;
	return (&device->connection == b);
}

static void _gattlib_connection_is_connected(gpointer data, gpointer user_data) {
	gattlib_adapter_t* adapter = data;
	struct _connection_is_connected* connection_is_connected = user_data;

	GSList *device_entry = g_slist_find_custom(adapter->devices, user_data, _is_device_connection);
	if (device_entry == NULL) {
		return;
	}

	gattlib_device_t* device = device_entry->data;
	connection_is_connected->is_connected = (device->state == CONNECTED);
}

bool gattlib_connection_is_connected(gattlib_connection_t* connection) {
    struct _connection_is_connected connection_is_connected = {
        .connection = connection,
        .is_connected = false
    };

    g_rec_mutex_lock(&m_gattlib_mutex);
    g_slist_foreach(m_adapter_list, _gattlib_connection_is_connected, &connection_is_connected);
    g_rec_mutex_unlock(&m_gattlib_mutex);

    return connection_is_connected.is_connected;
}