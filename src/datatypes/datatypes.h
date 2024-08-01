#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include <stdio.h>
#include <stdbool.h>

#define DEVICE_MSG_LEN 100 // some placeholder value for now.

typedef struct device_shadow
{
	double latitude;
	double longitude;
	double altitude; // skeptical
	int batt_voltage;
	bool led1_state; // false = off, true = on

} device_shadow_t;

/* @brief Just an snprintf wrapper.
    Returns the number of characters that would have been written if n had been sufficiently large, not counting the terminating null character.
    If an encoding error occurs, a negative number is returned.
    Notice that only when this returned value is non-negative and less than n, the string has been completely written.
*/
int device_to_json(char *json_payload, uint8_t payload_len, device_shadow_t device);


#endif /* _DATATYPES_H_ */
