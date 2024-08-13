// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_CHARGER_HUAWEI

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include <AsyncJson.h>

class WebApiHuaweiClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void getJsonData(JsonVariant& root);

private:
    void onStatus(AsyncWebServerRequest* request);
    void onAdminGet(AsyncWebServerRequest* request);
    void onAdminPost(AsyncWebServerRequest* request);
    void onPost(AsyncWebServerRequest* request);
};

#endif
