/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/mqtt.h>

#include "datatypes/datatypes.h"
#include "mqtt/mqtt_connection.h"
#include "gnss/gnss.h"

/* The mqtt client struct */
static struct mqtt_client client;
/* File descriptor */
static struct pollfd fds;

static K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(nrf9160_mqtt_simple, LOG_LEVEL_INF);

device_shadow_t g_device_state = 
{
	.latitude = 0,
	.longitude = 0,
	.altitude = 0,
	.batt_voltage = 0,
	.led1_state = false
};

bool g_psm_granted = false;
bool g_edrx_granted = false;

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			break;
		}
		LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
		k_sem_give(&lte_connected);
		break;

	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;

	/* On event PSM update, print PSM paramters and check if was enabled */
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_INF("PSM parameter update: Periodic TAU: %d s, Active time: %d s",
				evt->psm_cfg.tau, evt->psm_cfg.active_time);
		if (evt->psm_cfg.active_time == -1)
		{
			LOG_ERR("Network rejected PSM parameters. Failed to enable PSM");
		}
		else
		{
			g_psm_granted = true;
		}
		break;
	/* On event eDRX update, print eDRX paramters */
	case LTE_LC_EVT_EDRX_UPDATE:
		g_edrx_granted = true;
		LOG_INF("eDRX parameter update: eDRX: %f, PTW: %f",
				(double)evt->edrx_cfg.edrx, (double)evt->edrx_cfg.ptw);
		break;
	default:
		break;
	}
}

static int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err)
	{
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

/* lte_lc_init deprecated in >= v2.6.0 */
#if NCS_VERSION_NUMBER < 0x20600
	err = lte_lc_init();
	if (err)
	{
		LOG_ERR("Failed to initialize LTE link control library, error: %d", err);
		return err;
	}
#endif

	/* Request PSM and eDRX from the network */
	err = lte_lc_psm_req(true);
	if (err)
	{
		LOG_ERR("lte_lc_psm_req, error: %d", err);
	}

	err = lte_lc_edrx_req(true);
	if (err)
	{
		LOG_ERR("lte_lc_edrx_req, error: %d", err);
	}

	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err)
	{
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	int err;
	int json_len;
	bool stock_message = false;

	uint8_t json_payload[DEVICE_MSG_LEN];

	json_len = device_to_json(json_payload, DEVICE_MSG_LEN, g_device_state);
	if (json_len >= DEVICE_MSG_LEN || json_len < 0)
	{
		stock_message = true;
		LOG_ERR("Failure in json packet creation");
		LOG_ERR("Tried to make len %d msg %s", json_len, json_payload);
	}

	switch (has_changed)
	{
	case DK_BTN1_MSK:
		/* When button 1 is pressed, call data_publish() to publish lat/long */
		if (button_state & DK_BTN1_MSK)
		{
			if (stock_message)
			{
				err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
								   CONFIG_BUTTON_EVENT_PUBLISH_MSG, sizeof(CONFIG_BUTTON_EVENT_PUBLISH_MSG) - 1);
				if (err)
				{
					LOG_INF("Failed to send message, %d", err);
					return;
				}
			}
			else
			{
				err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
								   json_payload, json_len);
			}
		}
		break;
	}
}

static int mqtt_try_connect(void)
{
	int err;
	uint32_t connect_attempt = 0;

	while (1)
	{
		if (connect_attempt++ > 0)
		{
			LOG_INF("Reconnecting in %d seconds...",
					CONFIG_MQTT_RECONNECT_DELAY_S);
			k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
		}
		LOG_INF("Connection to broker using mqtt_connect");
		err = mqtt_connect(&client);
		if (err)
		{
			LOG_ERR("Error in mqtt_connect: %d, retrying...", err);
		}
		else
		{
			break;
		}
	}
	err = fds_init(&client, &fds);
	if (err)
	{
		LOG_ERR("Error in fds_init: %d", err);
		return 0;
	}
	return 0;
}

static int mqtt_connection(void)
{
	int err;
	err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
	if (err < 0)
	{
		LOG_ERR("Error in poll(): %d", errno);
		return -1;
	}

	err = mqtt_live(&client);
	if ((err != 0) && (err != -EAGAIN))
	{
		LOG_ERR("Error in mqtt_live: %d", err);
		return -2;
	}

	if ((fds.revents & POLLIN) == POLLIN)
	{
		err = mqtt_input(&client);
		if (err != 0)
		{
			LOG_ERR("Error in mqtt_input: %d", err);
			return -3;
		}
	}

	if ((fds.revents & POLLERR) == POLLERR)
	{
		LOG_ERR("POLLERR");
		return -4;
	}

	if ((fds.revents & POLLNVAL) == POLLNVAL)
	{
		LOG_ERR("POLLNVAL");
		return -5;
	}

	// success
	return 0;
}

int main(void)
{
	int err;

	if (dk_leds_init() != 0)
	{
		LOG_ERR("Failed to initialize the LED library");
	}

	err = modem_configure();
	if (err)
	{
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	if (dk_buttons_init(button_handler) != 0)
	{
		LOG_ERR("Failed to initialize the buttons library");
	}

	err = client_init(&client);
	if (err)
	{
		LOG_ERR("Failed to initialize MQTT client: %d", err);
		return 0;
	}

	mqtt_try_connect();

	LOG_DBG("PSM: %d EDRX %d", g_psm_granted, g_edrx_granted);
	err = gnss_init_and_start();
	if (err != 0)
	{
		LOG_ERR("Failed to initialize and start GNSS");
		return 0;
	}

	int mqtt_err = 0; // >=0 success
	while (1)		  // main application loop
	{
		while (mqtt_err >= 0) // connection loop
		{
			mqtt_err = mqtt_connection();
		}
		// disconnect then reconnect.
		if (mqtt_err < 0)
		{
			mqtt_err = 0; // reset flag
			LOG_INF("Disconnecting MQTT client");

			err = mqtt_disconnect(&client);
			if (err)
			{
				LOG_ERR("Could not disconnect MQTT client: %d", err);
			}
			mqtt_try_connect(); // reconnect
		}
	}

	/* This is never reached */
	return 0;
}
