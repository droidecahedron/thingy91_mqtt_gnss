#include <stdio.h>
#include <stdbool.h>

#include "datatypes.h"

int device_to_json(char *json_payload, uint8_t payload_len, device_shadow_t device)
{
	return snprintf(json_payload, payload_len, "{\"9160\": [{\"lat\": %.2f},{\"long\": \"%.2f\"},{\"alt\": \"%.2f\"},{\"battery\": \"%d %%\"},{\"led\": \"%s\"}]}",
												device.latitude, device.longitude, device.altitude, device.batt_voltage, device.led1_state ? "on" : "off");
}
 