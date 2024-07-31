#include <stdio.h>
#include <ncs_version.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <nrf_modem_gnss.h>

#include "gnss.h"
#include "../datatypes/datatypes.h"

extern device_shadow_t g_device_state;

static struct nrf_modem_gnss_pvt_data_frame pvt_data;

/* helper variables to find the TTFF */
static int64_t gnss_start_time;
static bool first_fix = false;

static uint8_t g_gps_data[MESSAGE_SIZE];

extern const struct k_sem lte_connected; // main.c handles the LTE work, so we need the semaphore from there. This is what Zephyr docs recommended.
extern bool g_psm_granted;
extern bool g_edrx_granted;

LOG_MODULE_REGISTER(gnss, LOG_LEVEL_INF);

/**@brief log fix data in a readable format
 */
static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
    LOG_INF("Latitude:       %.06f", pvt_data->latitude);
    LOG_INF("Longitude:      %.06f", pvt_data->longitude);
    LOG_INF("Altitude:       %.01f m", (double)pvt_data->altitude);
    LOG_INF("Time (UTC):     %02u:%02u:%02u.%03u",
            pvt_data->datetime.hour,
            pvt_data->datetime.minute,
            pvt_data->datetime.seconds,
            pvt_data->datetime.ms);

    int err = snprintf(g_gps_data, MESSAGE_SIZE, "Latitude: %.06f, Longitude: %.06f", pvt_data->latitude, pvt_data->longitude);
    if (err < 0)
    {
        LOG_ERR("Failed to print to buffer: %d", err);
    }
}

static void gnss_event_handler(int event)
{
    int err;

    switch (event)
    {
    /* On a PVT event, confirm if PVT data is a valid fix */
    case NRF_MODEM_GNSS_EVT_PVT:
        LOG_INF("Searching...");
        /* Print satellite information */
        int num_satellites = 0;
        for (int i = 0; i < 12; i++)
        {
            if (pvt_data.sv[i].signal != 0)
            {
                LOG_INF("sv: %d, cn0: %d, signal: %d", pvt_data.sv[i].sv, pvt_data.sv[i].cn0, pvt_data.sv[i].signal);
                num_satellites++;
            }
        }
        LOG_INF("Number of current satellites: %d", num_satellites);
        err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
        if (err)
        {
            LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
            return;
        }
        if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID)
        {
            dk_set_led_on(DK_LED1);
            print_fix_data(&pvt_data);
            /* Print the time to first fix */
            if (!first_fix)
            {
                LOG_INF("Time to first fix: %2.1lld s", (k_uptime_get() - gnss_start_time) / 1000);
                first_fix = true;
            }
            return;
        }
        if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED)
        {
            LOG_INF("GNSS blocked by LTE activity");
        }
        else if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME)
        {
            LOG_INF("Insufficient GNSS time windows");
        }
        break;
    /* Log when the GNSS sleeps and wakes up */
    case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
        LOG_INF("GNSS has woken up");
        break;
    case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
        LOG_INF("GNSS enter sleep after fix");
        break;
    default:
        break;
    }
}

int gnss_init_and_start(void)
{

    /* Set the modem mode to normal */
    if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL) != 0)
    {
        LOG_ERR("Failed to activate GNSS functional mode");
        return -1;
    }

    if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0)
    {
        LOG_ERR("Failed to set GNSS event handler");
        return -1;
    }

    if (nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0)
    {
        LOG_ERR("Failed to set GNSS fix interval");
        return -1;
    }

    if (nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0)
    {
        LOG_ERR("Failed to set GNSS fix retry");
        return -1;
    }

    LOG_INF("Starting GNSS");
    if (nrf_modem_gnss_start() != 0)
    {
        LOG_ERR("Failed to start GNSS");
        return -1;
    }

    // If not granted psm/edrx, we'll never get a fix. Depends on network. This will give GNSS priority over LTE events.
    // An alternative would be to manaully activate and deactivate emodem when wanting to use GNSS in main.c
    if (!g_edrx_granted && !g_psm_granted)
    {
        if (nrf_modem_gnss_prio_mode_enable() != 0)
        {
            LOG_ERR("Failed to set priority mode for GNSS");
        }
        else
        {
            LOG_INF("GNSS priority mode set");
        }
    }

    gnss_start_time = k_uptime_get();

    return 0;
}