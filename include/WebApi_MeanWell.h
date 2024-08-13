// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_CHARGER_MEANWELL

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiMeanWellClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onStatus(AsyncWebServerRequest* request);
    void onPowerGet(AsyncWebServerRequest* request);
    void onPowerPost(AsyncWebServerRequest* request);

    void onLimitGet(AsyncWebServerRequest* request);
    void onLimitPost(AsyncWebServerRequest* request);

    void onAdminGet(AsyncWebServerRequest* request);
    void onAdminPost(AsyncWebServerRequest* request);
};

#endif
