// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#if defined(USE_CHARGER_MEANWELL) && defined(USE_CHARGER_HUAWEI)
#error "Meanwell and Huawei charger are not supported at the same time. Please decide for one of both"
#endif

#define SERIAL_BAUDRATE 115200

#define APP_HOSTNAME "OpenDTU-%06X"

#define HTTP_PORT 80

#define ACCESS_POINT_NAME "OpenDTU-"
#define ACCESS_POINT_PASSWORD "openDTU42"
#define ACCESS_POINT_TIMEOUT 3;
#define AUTH_USERNAME "admin"
#define SECURITY_ALLOW_READONLY true

#define WIFI_RECONNECT_TIMEOUT 30
#define WIFI_RECONNECT_REDO_TIMEOUT 600

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_DHCP true

#define MDNS_ENABLED false

#define SYSLOG_ENABLED false
#define SYSLOG_PORT 514

#define MODBUS_ENABLED false
#define MODBUS_DELAY_START false
#define MODBUS_MFRNAME "OpenDTU"
#define MODBUS_MODELNAME "OpenDTU-SunSpec"
#define MODBUS_OPTIONS ""
#define MODBUS_VERSION "1.0"

#define NTP_SERVER "opendtu.pool.ntp.org"
#define NTP_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_TIMEZONEDESCR "Europe/Berlin"
#define NTP_LONGITUDE 10.4515f
#define NTP_LATITUDE 51.1657f
#define NTP_SUNSETTYPE 1

#define MQTT_ENABLED false
#define MQTT_HOST ""
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_TOPIC "solar/"
#define MQTT_RETAIN true
#define MQTT_TLS false
// ISRG_Root_X1.crt -- Root CA for Letsencrypt
#define MQTT_ROOT_CA_CERT "-----BEGIN CERTIFICATE-----\n"                                      \
                          "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
                          "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
                          "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
                          "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
                          "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
                          "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
                          "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
                          "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
                          "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
                          "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
                          "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
                          "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
                          "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n" \
                          "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n" \
                          "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n" \
                          "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n" \
                          "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n" \
                          "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n" \
                          "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n" \
                          "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n" \
                          "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n" \
                          "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n" \
                          "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n" \
                          "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n" \
                          "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n" \
                          "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n" \
                          "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n" \
                          "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n" \
                          "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n" \
                          "-----END CERTIFICATE-----\n"
#define MQTT_TLSCERTLOGIN false
#define MQTT_TLSCLIENTCERT ""
#define MQTT_TLSCLIENTKEY ""
#define MQTT_LWT_TOPIC "dtu/status"
#define MQTT_LWT_ONLINE "online"
#define MQTT_LWT_OFFLINE "offline"
#define MQTT_LWT_QOS 2U
#define MQTT_PUBLISH_INTERVAL 5U
#define MQTT_CLEAN_SESSION true

#define DTU_SERIAL 0x99978563412
#define DTU_POLLINTERVAL 5U
#define DTU_NRF_PA_LEVEL 0
#define DTU_CMT_PA_LEVEL 0
#define DTU_CMT_FREQUENCY 865000000U
#define DTU_CMT_COUNTRY_MODE 0U

#define MQTT_HASS_ENABLED false
#define MQTT_HASS_EXPIRE true
#define MQTT_HASS_RETAIN true
#define MQTT_HASS_TOPIC "homeassistant/"
#define MQTT_HASS_INDIVIDUALPANELS false

#define DEV_PINMAPPING ""

#define DISPLAY_POWERSAFE true
#define DISPLAY_SCREENSAVER true
#define DISPLAY_ROTATION 2
#define DISPLAY_CONTRAST 60U
#define DISPLAY_LANGUAGE 0U
#define DISPLAY_DIAGRAM_DURATION (10UL * 60UL * 60UL)
#define DISPLAY_DIAGRAM_MODE 1U

#define REACHABLE_THRESHOLD 2U

#define LED_BRIGHTNESS 100U

#define MAX_INVERTER_LIMIT 2250

#define VEDIRECT_ENABLED false
#define VEDIRECT_VERBOSE_LOGGING false
#define VEDIRECT_UPDATESONLY true

#ifdef USE_REFUsol_INVERTER
#define REFUsol_ENABLED false
#define REFUsol_UPDATESONLY true
#define REFUsol_POLLINTERVAL 5
#endif

#define POWERMETER_ENABLED false
#define POWERMETER_UPDATESONLY true
#define POWERMETER_POLLING_INTERVAL 10
#define POWERMETER_SOURCE 2
#define POWERMETER_SDMBAUDRATE 9600
#define POWERMETER_SDMADDRESS 1

#define HTTP_REQUEST_TIMEOUT_MS 1000

#define POWERLIMITER_ENABLED false
#define POWERLIMITER_UPDATESONLY true
#define POWERLIMITER_INTERVAL 10
#define POWERLIMITER_SOLAR_PASSTHROUGH_ENABLED true
#define POWERLIMITER_SOLAR_PASSTHROUGH_LOSSES 3
#define POWERLIMITER_BATTERY_ALWAYS_USE_AT_NIGHT false
#define POWERLIMITER_IS_INVERTER_BEHIND_POWER_METER true
#define POWERLIMITER_IS_INVERTER_SOLAR_POWERED false
#define POWERLIMITER_USE_OVERSCALING_TO_COMPENSATE_SHADING false
#define POWERLIMITER_INVERTER_ID 0
#define POWERLIMITER_INVERTER_CHANNEL_ID 0
#define POWERLIMITER_TARGET_POWER_CONSUMPTION 0
#define POWERLIMITER_TARGET_POWER_CONSUMPTION_HYSTERESIS 0
#define POWERLIMITER_LOWER_POWER_LIMIT 10
#define POWERLIMITER_BASE_LOAD_LIMIT 100
#define POWERLIMITER_UPPER_POWER_LIMIT 800
#define POWERLIMITER_IGNORE_SOC false
#define POWERLIMITER_BATTERY_SOC_START_THRESHOLD 80
#define POWERLIMITER_BATTERY_SOC_STOP_THRESHOLD 20
#define POWERLIMITER_VOLTAGE_START_THRESHOLD 50.0
#define POWERLIMITER_VOLTAGE_STOP_THRESHOLD 49.0
#define POWERLIMITER_VOLTAGE_LOAD_CORRECTION_FACTOR 0.001
#define POWERLIMITER_RESTART_HOUR -1
#define POWERLIMITER_FULL_SOLAR_PASSTHROUGH_SOC 100
#define POWERLIMITER_FULL_SOLAR_PASSTHROUGH_START_VOLTAGE 100
#define POWERLIMITER_FULL_SOLAR_PASSTHROUGH_STOP_VOLTAGE 100

#define MAX_BATTERIES   2   // currently only 2 Pylontech Batteries are supported (Master and one Slave)
#define BATTERY_ENABLED false
#define BATTERY_PROVIDER 0 // Pylontech RS485 receiver
#define BATTERY_ENABLE_DISCHARGE_CURRENT_LIMIT false
#define BATTERY_DISCHARGE_CURRENT_LIMIT 0
#define BATTERY_USE_BATTERY_REPORTED_DISCHARGE_CURRENT_LIMIT false
#define BATTERY_ZENDURE_DEVICE 0
#define BATTERY_ZENDURE_MIN_SOC 0
#define BATTERY_ZENDURE_MAX_SOC 100
#define BATTERY_ZENDURE_BYPASS_MODE 0
#define BATTERY_ZENDURE_MAX_OUTPUT 800

#define BATTERY_POLLINTERVAL 7
#define BATTERY_UPDATESONLY true
#define BATTERY_MIN_DISCHARGE_TEMPERATURE -10
#define BATTERY_MAX_DISCHARGE_TEMPERATURE 50
#define BATTERY_MIN_CHARGE_TEMPERATURE 0
#define BATTERY_MAX_CHARGE_TEMPERATURE 50

#define MCP2515_CAN_CONTROLLER_FREQUENCY 8000000UL

#ifdef USE_CHARGER_HUAWEI
#define HUAWEI_ENABLED false
#define HUAWEI_AUTO_POWER_VOLTAGE_LIMIT 42.0
#define HUAWEI_AUTO_POWER_ENABLE_VOLTAGE_LIMIT 42.0
#define HUAWEI_AUTO_POWER_LOWER_POWER_LIMIT 150
#define HUAWEI_AUTO_POWER_UPPER_POWER_LIMIT 2000
#define HUAWEI_AUTO_POWER_STOP_BATTERYSOC_THRESHOLD 95
#define HUAWEI_AUTO_POWER_TARGET_POWER_CONSUMPTION 0
#endif
#ifdef USE_CHARGER_MEANWELL
#define MEANWELL_ENABLED false
#define MEANWELL_UPDATESONLY true
#define MEANWELL_POLLINTERVAL 5
#define MEANWELL_MINVOLTAGE 42.0
#define MEANWELL_MAXVOLTAGE 54.0
#define MEANWELL_MINCURRENT 1.0
#define MEANWELL_MAXCURRENT 18.0
#define MEANWELL_HYSTERESIS 25.0
#endif

#define ZERO_EXPORT_ENABLED false
#define ZERO_EXPORT_UPDATESONLY true
#define ZERO_EXPORT_INVERTER_ID 0
#define ZERO_EXPORT_POWER_HYSTERESIS 2 // Hysteresis 2%
#define ZERO_EXPORT_MAX_GRID 400 // max 400 Watt Grid Export
#define ZERO_EXPORT_MINIMUM_LIMIT 10 // 10% minimum Inverter
#define ZERO_EXPORT_TN 60 // 60 Seconds time constant for PID controller
