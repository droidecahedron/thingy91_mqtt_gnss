#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_ADP536X)
#include <adp536x.h>
#endif

#include "pmic.h"
#include "../datatypes/datatypes.h"

#define ADP536X_I2C_DEVICE DEVICE_DT_GET(DT_NODELABEL(i2c2))

extern device_shadow_t g_device_state;
extern bool g_started_up; // don't feel like using any sync primitives.

LOG_MODULE_REGISTER(pmic, LOG_LEVEL_INF);

// Battery Sampling: timer will fire a battery charge request work queue item every time it finishes.
//! WorkQ
static struct k_work battery_soc_sample_work;
static void battery_soc_sample_work_fn(struct k_work *work)
{
    uint8_t battery_percentage_timer;
    adp536x_fg_soc(&battery_percentage_timer);
    LOG_INF("Batt percentage as uint8 : %d", battery_percentage_timer);
    g_device_state.batt_voltage = battery_percentage_timer;
}

//! Timer
static void battery_sample_timer_handler(struct k_timer *timer);
K_TIMER_DEFINE(battery_sample_timer, battery_sample_timer_handler, NULL);

void battery_sample_timer_handler(struct k_timer *timer)
{
    k_work_init(&battery_soc_sample_work, battery_soc_sample_work_fn);
    k_work_submit(&battery_soc_sample_work);
}

// pmic init
static int power_mgmt_init(void)
{
    int err;

    err = adp536x_init(ADP536X_I2C_DEVICE);
    if (err)
    {
        LOG_ERR("ADP536X failed to initialize, error: %d\n", err);
        return err;
    }

    /* Sets the VBUS current limit to 500 mA. */
    err = adp536x_vbus_current_set(ADP536X_VBUS_ILIM_500mA);
    if (err)
    {
        LOG_ERR("Could not set VBUS current limit, error: %d\n", err);
        return err;
    }

    /* Sets the charging current to 320 mA. */
    err = adp536x_charger_current_set(ADP536X_CHG_CURRENT_320mA);
    if (err)
    {
        LOG_ERR("Could not set charging current, error: %d\n", err);
        return err;
    }

    /* Sets the charge current protection threshold to 400 mA. */
    err = adp536x_oc_chg_current_set(ADP536X_OC_CHG_THRESHOLD_400mA);
    if (err)
    {
        LOG_ERR("Could not set charge current protection, error: %d\n",
                err);
        return err;
    }

    err = adp536x_charging_enable(true);
    if (err)
    {
        LOG_ERR("Could not enable charging: %d\n", err);
        return err;
    }

    err = adp536x_fg_set_mode(ADP566X_FG_ENABLED, ADP566X_FG_MODE_SLEEP);
    if (err)
    {
        LOG_ERR("Could not enable fuel gauge: %d", err);
        return err;
    }

    return 0;
}

static int thingy91_board_init(void)
{
    int err;

    err = power_mgmt_init();
    if (err)
    {
        LOG_ERR("power_mgmt_init failed with error: %d", err);
        return err;
    }
    g_started_up = true;
    return 0;
}

int startup_thread(void)
{
    LOG_INF("STARTUP Thread: Not using SYS_INIT for pmic inits");
    if (thingy91_board_init() != 0)
    {
        LOG_ERR("Could not initialize PMIC");
    }
    LOG_INF("STARTUP thread starting Software timer to sample battery");
    uint8_t battery_percentage_startup;
    adp536x_fg_soc(&battery_percentage_startup); // quick read when starting up
    LOG_INF("Batt percentage on startup: %d", battery_percentage_startup);
    return 0;
}

// thread just exists to enable the pmic timer.
int pmic_thread(void)
{
    while (!g_started_up) // wait for g_started_up to go true, either by sys init or the startup thread.
    {
        k_yield();
    }
    k_timer_start(&battery_sample_timer, K_MSEC(1000), K_MSEC(BATTERY_SAMPLE_INTERVAL_MS));
    return 0;
}

// You can uncomment this if you want. I think it's odd to do this way, but atv2 does it with sys init.
#if (DEBUG_USE_SYSINIT)
SYS_INIT(thingy91_board_init, POST_KERNEL, CONFIG_BOARD_INIT_PRIORITY);
#else
K_THREAD_DEFINE(startup_thread_id, STACKSIZE, startup_thread, NULL, NULL, NULL,
                STARTUP_THREAD_PRIORITY, 0, 0);
#endif

K_THREAD_DEFINE(pmic_thread_id, STACKSIZE, pmic_thread, NULL, NULL, NULL,
                PMIC_THREAD_PRIORITY, 0, 0);
