// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_REFUsol_INVERTER

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiREFUsolClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onREFUsolStatus(AsyncWebServerRequest* request);
    void onREFUsolAdminGet(AsyncWebServerRequest* request);
    void onREFUsolAdminPost(AsyncWebServerRequest* request);
};
#endif
