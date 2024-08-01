# thingy91_mqtt_simple

## hardware / documentation
- Thingy91
  
  ![image](https://github.com/droidecahedron/thingy91_adp5360_simple/assets/63935881/22f5c0fe-d8a3-462c-ace9-84259d406d86)

- [Web manpage](https://docs.nordicsemi.com/category/thingy91-category)
- [Downloads (incl. schematic)](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91/Download)

*Should work on a 9160dk, but you need to remove the pmic config in `prj.conf`*

## Overview
This example code monitors the state of LED1, battery percentage (via thingy91 onboard pmic), and geographical location.

It connects to an mqtt broker of your choice and you can publish/subscribe to an arbitrary topic. It's an extension of the Nordic Developer Academy for cellular fundamentals [link](https://academy.nordicsemi.com/courses/cellular-iot-fundamentals), since there is a UDP+GNSS, COAP+GNSS, but not an MQTT+GNSS. Asset Tracker v2 is a bit dense and connects to nRF Cloud, this is a stripped down (unofficial) example. The academy covers how to add TLS, I did not feel it necessary here.

##  Usage
Select your endpoint via prj.conf and the kconfigs `CONFIG_MQTT_PUB_TOPIC` and `CONFIG_MQTT_SUB_TOPIC`.

You can publish to whatever you configure the sub topic to in order to control the state of LED1 on the device. Simply publish `LED1ON` OR `LED1OFF`.

If you want to run this on a 9160DK, you need to remove the PMIC module and disable the kconfig `CONFIG_ADP536X`.

You will want to monitor the logs to see when you get your first fix, until then lat/long/alt default to 0 as the device does not know where it is yet. There will be a log stating the coordinates and that the module is going to sleep.

Push the button to upload a device state json string to your endpoint broker. If the orange cover is on, it is flexible so you can also push down on the Nordic logo.

![image](https://github.com/user-attachments/assets/7f5871e3-0b26-4e75-9673-72441118c226)


All modules modify a global data
Module | Function
--|--
main | Initialization and main connection logic
mqtt | mqtt connection implementation
gnss | modem configurations and locationing logic
pmic | (thingy91 specific) pmic initialization and periodic sampling [1]
datatypes | struct for holding system data variables, and basic json snprintf for the struct. [2]

> [1] :  You should use a lib for json (like coreJSON) but this gets the job done. JSON in general is a bit much for constrained devices, but aws likes it so I've just opted to manually craft the string.

> [2] : Thingy91 has an ADP5360 PMIC (shame it's not a Nordic nPM1300), but the atv2's sensor module does not init the device, it happens as a board init via SYS_INIT. This sample shows init and using it via start-up thread, or via SYS_INIT like the atv2/thingy91 board init does.
> You change the `DEBUG_USE_SYSINIT` define in `pmic.h` to true/false depending on how you want it to swing. `thingy91_board_init` is broken out from the board init in the SDK which has a `SYS_INIT` that gets called. [**Here**](https://github.com/droidecahedron/thingy91_adp5360_simple/assets/63935881/9b8076cf-b1c9-422e-8dfe-1ba4d923207c) is a handy diagram for `SYS_INIT` that I like to refer to.
> 
> `SYS_INIT` a handy thing to read about and potentially use in case you are working on controlling something in both bootloader and application, and run into strange cases of application driver inits wiping any work done in the boot. (For example, having a pin stay high from boot to a specific execution in application). However, in the case of atv2 and peeling functionality out, it can make it harder to reason about what the code is doing. It also complicates error handling a bit as well.
> Further reading here: [Zephyr project SYS_INIT doc](https://docs.zephyrproject.org/latest/doxygen/html/group__sys__init.html), [NCS Intermediate](https://academy.nordicsemi.com/courses/nrf-connect-sdk-intermediate/lessons/lesson-1-zephyr-rtos-advanced/topic/boot-up-sequence-execution-context/)

## Terminal Shots

**Device Side**

![image](https://github.com/user-attachments/assets/2db4bb38-33f3-4d79-b82e-be78ffd139d2)

**Server side (via MQTT Explorer)**

![image](https://github.com/user-attachments/assets/17ae83d8-2224-4c96-8cff-e0c70c3f626e)

**Controlling LED**

![image](https://github.com/user-attachments/assets/d0f21db9-60b3-41bb-bd54-b10dfb08b031)

![image](https://github.com/user-attachments/assets/82115deb-b2e4-48bc-bc44-4949ad5d17e7)

