#include <stdio.h>
#include <stdbool.h>

#include "datatypes.h"

void device_to_json(char *json_payload, uint8_t payload_len, device_shadow_t device)
{
	snprintf(json_payload, payload_len, "{\"9160\": [\
											{\"lat\": %f},\
											{\"long\": \"%f\"},\
											{\"alt\": \"%f\"},\
											{\"battery\": \"%dV\"},\
											{\"led\": \"%s\"}\
											]}",
											device.latitude,
											device.longitude,
											device.altitude,
											device.batt_voltage,
											device.led1_state ? "on" : "off");
}
