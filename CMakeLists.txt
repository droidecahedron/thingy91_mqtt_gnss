cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf9160_mqtt_simple)

# NORDIC SDK APP START
zephyr_include_directories(mqtt)
target_sources(app PRIVATE src/main.c
            src/mqtt/mqtt_connection.c)
# NORDIC SDK APP END