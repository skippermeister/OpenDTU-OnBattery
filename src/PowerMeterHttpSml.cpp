// SPDX-License-Identifier: GPL-2.0-or-later
#include "PowerMeterHttpSml.h"
#include "MessageOutput.h"
#include <WiFiClientSecure.h>
#include <base64.h>
#include <ESPmDNS.h>
#include "PowerMeter.h"

static constexpr char TAG[] ="[PowerMeterHttpSml]";

PowerMeterHttpSml::~PowerMeterHttpSml()
{
    _taskDone = false;

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = true;
    lock.unlock();

    _cv.notify_all();

    if (_taskHandle != nullptr) {
        while (!_taskDone) { delay(10); }
        _taskHandle = nullptr;
    }
}

bool PowerMeterHttpSml::init()
{
    _upHttpGetter = std::make_unique<HttpGetter>(_cfg.HttpRequest);

    if (_upHttpGetter->init()) { return true; }

    MessageOutput.printf("%s Initializing HTTP getter failed:\r\n", TAG);
    MessageOutput.printf("%s %s\r\n", TAG, _upHttpGetter->getErrorText());

    _upHttpGetter = nullptr;

    return false;
}

void PowerMeterHttpSml::loop()
{
    if (_taskHandle != nullptr) { return; }

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = false;
    lock.unlock();

    uint32_t constexpr stackSize = 3072;
    if (!xTaskCreate(PowerMeterHttpSml::pollingLoopHelper, "PM:HTTP+SML", stackSize, this, 1/*prio*/, &_taskHandle))
        MessageOutput.printf("%s error: creating PowerMeter Task\r\n", TAG);
}

void PowerMeterHttpSml::pollingLoopHelper(void* context)
{
    auto pInstance = static_cast<PowerMeterHttpSml*>(context);
    pInstance->pollingLoop();
    pInstance->_taskDone = true;
    vTaskDelete(nullptr);
}

void PowerMeterHttpSml::pollingLoop()
{
    std::unique_lock<std::mutex> lock(_pollingMutex);

    while (!_stopPolling) {
        auto elapsedMillis = millis() - _lastPoll;
        auto intervalMillis = _cfg.PollingInterval * 1000;
        if (_lastPoll > 0 && elapsedMillis < intervalMillis) {
            auto sleepMs = intervalMillis - elapsedMillis;
            _cv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPolling; }); // releases the mutex
            continue;
        }

        _lastPoll = millis();

        lock.unlock(); // polling can take quite some time
        auto res = poll();
        lock.lock();

        if (PowerMeter.getVerboseLogging()) {
            MessageOutput.printf("%s%s Round trip %lu ms\r\n", TAG, "task", millis() - _lastPoll);
        }

        if (!res.isEmpty()) {
            MessageOutput.printf("%s %s\r\n", TAG, res.c_str());
            continue;
        }

        if (PowerMeter.getVerboseLogging())
            MessageOutput.printf("%s New total: %.2fW\r\n", TAG, getPowerTotal());

        gotUpdate();
    }
}

bool PowerMeterHttpSml::isDataValid() const
{
    uint32_t age = millis() - getLastUpdate();
    return getLastUpdate() > 0 && (age < (3 * _cfg.PollingInterval * 1000));
}

String PowerMeterHttpSml::poll()
{
    if (!_upHttpGetter) {
        return "Initialization of HTTP request failed";
    }

    auto res = _upHttpGetter->performGetRequest();
    if (!res) {
        return _upHttpGetter->getErrorText();
    }

    auto pStream = res.getStream();
    if (!pStream) {
        return "Programmer error: HTTP request yields no stream";
    }

    while (pStream->available()) {
        processSmlByte(pStream->read());
    }

    PowerMeterSml::reset();

    return "";
}
