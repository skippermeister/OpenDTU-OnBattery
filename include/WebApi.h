// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ErrorMessages.h"
#include "WebApi_REFUsol.h"
#include "WebApi_ZeroExport.h"
#include "WebApi_battery.h"
#include "WebApi_config.h"
#include "WebApi_device.h"
#include "WebApi_devinfo.h"
#include "WebApi_dtu.h"
#include "WebApi_errors.h"
#include "WebApi_eventlog.h"
#include "WebApi_firmware.h"
#include "WebApi_gridprofile.h"
#include "WebApi_inverter.h"
#include "WebApi_limit.h"
#include "WebApi_maintenance.h"
#include "WebApi_mqtt.h"
#include "WebApi_network.h"
#include "WebApi_ntp.h"
#include "WebApi_power.h"
#include "WebApi_powerlimiter.h"
#include "WebApi_powermeter.h"
#include "WebApi_prometheus.h"
#include "WebApi_security.h"
#include "WebApi_sysstatus.h"
#include "WebApi_vedirect.h"
#include "WebApi_webapp.h"
#include "WebApi_ws_REFUsol_live.h"
#include "WebApi_ws_console.h"
#include "WebApi_ws_live.h"
#include "WebApi_ws_vedirect_live.h"
#ifdef CHARGER_HUAWEI
#include "WebApi_Huawei.h"
#include "WebApi_ws_Huawei.h"
#else
#include "WebApi_MeanWell.h"
#include "WebApi_ws_MeanWell.h"
#endif
#include "WebApi_ws_battery.h"
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiClass {
public:
    WebApiClass();
    void init(Scheduler& scheduler);

    static bool checkCredentials(AsyncWebServerRequest* request);
    static bool checkCredentialsReadonly(AsyncWebServerRequest* request);

    static void sendTooManyRequests(AsyncWebServerRequest* request);

    static void writeConfig(JsonVariant& retMsg, const WebApiError code = WebApiError::GenericSuccess, const String& message = "Settings saved!");

private:
    AsyncWebServer _server;

    WebApiBatteryClass _webApiBattery;
    WebApiConfigClass _webApiConfig;
    WebApiDeviceClass _webApiDevice;
    WebApiDevInfoClass _webApiDevInfo;
    WebApiDtuClass _webApiDtu;
    WebApiEventlogClass _webApiEventlog;
    WebApiFirmwareClass _webApiFirmware;
    WebApiGridProfileClass _webApiGridprofile;
    WebApiInverterClass _webApiInverter;
    WebApiLimitClass _webApiLimit;
    WebApiMaintenanceClass _webApiMaintenance;
    WebApiMqttClass _webApiMqtt;
    WebApiNetworkClass _webApiNetwork;
    WebApiNtpClass _webApiNtp;
    WebApiPowerClass _webApiPower;
    WebApiPowerMeterClass _webApiPowerMeter;
    WebApiPowerLimiterClass _webApiPowerLimiter;
    WebApiZeroExportClass _webApiZeroExport;
#ifdef USE_PROMETHEUS
    WebApiPrometheusClass _webApiPrometheus;
#endif
    WebApiSecurityClass _webApiSecurity;
    WebApiSysstatusClass _webApiSysstatus;
    WebApiWebappClass _webApiWebapp;
    WebApiWsConsoleClass _webApiWsConsole;
    WebApiWsLiveClass _webApiWsLive;
    WebApiWsVedirectLiveClass _webApiWsVedirectLive;
    WebApiVedirectClass _webApiVedirect;
#ifdef USE_REFUsol_INVERTER
    WebApiWsREFUsolLiveClass _webApiWsREFUsolLive;
    WebApiREFUsolClass _webApiREFUsol;
#endif
#ifdef CHARGER_HUAWEI
    WebApiHuaweiClass _webApiHuaweiClass;
    WebApiWsHuaweiLiveClass _webApiWsHuaweiLive;
#else
    WebApiMeanWellClass _webApiMeanWellClass;
    WebApiWsMeanWellLiveClass _webApiWsMeanWellLive;
#endif
    WebApiWsBatteryLiveClass _webApiWsBatteryLive;
};

extern WebApiClass WebApi;
