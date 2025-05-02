#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <drivers/bme68x_iaq.h>
#include "bme680.h"
#include "../datatypes/datatypes.h"
#include <zephyr/logging/log.h>

extern device_shadow_t g_device_state;
LOG_MODULE_REGISTER(bme680_module, LOG_LEVEL_INF);

int aqi_thread(void)
{
    const struct device *const dev = DEVICE_DT_GET_ONE(bosch_bme680);
    struct sensor_value temp, press, humidity, gas_res;

    if (!device_is_ready(dev))
    {
        printk("sensor: device not ready.\n");
        return 0;
    }

    while (1)
    {
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        
        // Ajout spécifique pour l'affichage température
        LOG_INF("[TEMPERATURE] %.2f C", sensor_value_to_double(&temp));  // Nouvelle ligne
        
        sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);
        sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
        sensor_channel_get(dev, SENSOR_CHAN_GAS_RES, &gas_res);

        LOG_INF("T: %d.%02d C; P: %d.%02d kPa; H: %d.%02d %% humid; G: %d.%02d ohms",
                temp.val1, temp.val2, press.val1, press.val2,
                humidity.val1, humidity.val2, gas_res.val1,
                gas_res.val2);

        g_device_state.temperature = temp.val1;
        g_device_state.pressure = press.val1;
        g_device_state.relative_humidity = humidity.val1;
        g_device_state.gas_res = gas_res.val1;

        k_sleep(K_MSEC(SENSOR_SAMPLE_INTERVAL_MS));
    }
    return 0;
}

K_THREAD_DEFINE(aqi_thread_id, STACKSIZE, aqi_thread, NULL, NULL, NULL, AQI_THREAD_PRIORITY, 0, 0);
