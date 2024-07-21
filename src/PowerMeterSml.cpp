// SPDX-License-Identifier: GPL-2.0-or-later
#include "PowerMeterSml.h"
#include "MessageOutput.h"
#include "PowerMeter.h"
#include "Datastore.h"

float PowerMeterSml::getHousePower(void) const
{
    float value = getPowerTotal();
    return value + Datastore.getTotalAcPowerEnabled();
}

float PowerMeterSml::getPowerTotal() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (_values.activePowerTotal.has_value()) { return *_values.activePowerTotal; }
    return 0;
}

void PowerMeterSml::doMqttPublish() const
{
#define PUB(t, m) \
    if (_values.m.has_value()) { mqttPublish(t, *_values.m); }

    std::lock_guard<std::mutex> l(_mutex);
    PUB("power1", activePowerL1);
    PUB("power2", activePowerL2);
    PUB("power3", activePowerL3);
    PUB("voltage1", voltageL1);
    PUB("voltage2", voltageL2);
    PUB("voltage3", voltageL3);
    PUB("current1", currentL1);
    PUB("current2", currentL2);
    PUB("current3", currentL3);
    PUB("import", energyImport);
    PUB("export", energyExport);

#undef PUB
}

void PowerMeterSml::reset()
{
    smlReset();
    _cache = { std::nullopt };
}

void PowerMeterSml::processSmlByte(uint8_t byte)
{
    switch (smlState(byte)) {
        case SML_LISTEND:
            for (auto& handler: smlHandlerList) {
                if (!smlOBISCheck(handler.OBIS)) { continue; }

                float helper = 0.0;
                handler.decoder(helper);

                if (PowerMeter.getVerboseLogging()) {
                    MessageOutput.printf("[%s] decoded %s to %.2f\r\n",
                            _user.c_str(), handler.name, helper);
                }

                std::lock_guard<std::mutex> l(_mutex);
                *handler.target = helper;
            }
            break;
        case SML_FINAL:
            gotUpdate();
            _values = _cache;
            reset();
            MessageOutput.printf("[%s] TotalPower: %5.2f\r\n",
                    _user.c_str(), getPowerTotal());
            break;
        case SML_CHECKSUM_ERROR:
            reset();
            MessageOutput.printf("[%s] checksum verification failed\r\n",
                    _user.c_str());
            break;
        default:
            break;
    }
}
