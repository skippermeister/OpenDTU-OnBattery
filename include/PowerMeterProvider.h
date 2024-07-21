// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include "Configuration.h"

class PowerMeterProvider {
public:
    virtual ~PowerMeterProvider() { }

    enum class Type : unsigned {
        MQTT = 0,
        SDM1PH = 1,
        SDM3PH = 2,
        HTTP_JSON = 3,
        SERIAL_SML = 4,
        SMAHM2 = 5,
        HTTP_SML = 6
    };

    // returns true if the provider is ready for use, false otherwise
    virtual bool init() = 0;

    virtual void loop() = 0;
    virtual float getPowerTotal() const = 0;
    virtual float getHousePower() const = 0;
    virtual bool isDataValid() const;

    uint32_t getLastUpdate() const { return _lastUpdate; }
    void mqttLoop() const;

protected:
    PowerMeterProvider() {
    }

    void gotUpdate() { _lastUpdate = millis(); }

    void mqttPublish(String const& topic, float const& value) const;

private:
    virtual void doMqttPublish() const = 0;

    // gotUpdate() updates this variable potentially from a different thread
    // than users that request to read this variable through getLastUpdate().
    std::atomic<uint32_t> _lastUpdate = 0;

    mutable uint32_t _lastMqttPublish = 0;
};
