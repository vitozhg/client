

#ifndef _G_CONFIG_H_
#define _G_CONFIG_H_
#include "defines.h"
#include "nal_api.h"

#define MBED_OS_EXAMPLE_WAKAAMA
/******************************************************************************
* NAL-Network Adaptation Layer
******************************************************************************/
//#define NETWORK_SUPPART_ETHERNET
#define NETWORK_SUPPORT_WIFI
//#define NETWORK_SUPPORT_NBIOT


/* WIFI DEFINATIONS */
#ifdef NETWORK_SUPPORT_WIFI

#ifndef MBED_CFG_ESP8266_TX
#define MBED_CFG_ESP8266_TX I2C_SDA1
#endif

#ifndef MBED_CFG_ESP8266_RX
#define MBED_CFG_ESP8266_RX I2C_SCL1
#endif

#ifndef MBED_CFG_ESP8266_DEBUG
#define MBED_CFG_ESP8266_DEBUG true
#endif

#ifndef MBED_CFG_ESP8266_SSID
//#define MBED_CFG_ESP8266_SSID "TERENCE_2G"
#define MBED_CFG_ESP8266_SSID "tinycloud"
#endif

#ifndef MBED_CFG_ESP8266_PASS
//#define MBED_CFG_ESP8266_PASS "19810725"
#define MBED_CFG_ESP8266_PASS "TinycloudDemo"
#endif

//#define MBED_CFG_ESP8266_TCP

#endif //#ifdef NETWORK_SUPPORT_WIFI

/******************************************************************************
* APL-Application Layer
******************************************************************************/
#define ENABLE_PWM_RGB_LED

#endif //_G_CONFIG_H_