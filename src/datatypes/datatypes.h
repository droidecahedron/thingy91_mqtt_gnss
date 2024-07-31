#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include <stdio.h>
#include <stdbool.h>

typedef struct device_shadow
{
	double latitude;
	double longitude;
	double altitude; // skeptical
	int batt_voltage;
	bool led1_state; // false = off, true = on

} device_shadow_t;

void device_to_json(char *json_payload, uint8_t payload_len, device_shadow_t device);


#endif /* _DATATYPES_H_ */
