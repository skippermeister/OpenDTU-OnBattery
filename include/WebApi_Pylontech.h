// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiPylontechClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

    //    void getJsonData(JsonObject& root);

private:
    void onStatus(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};
