// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYLONTECH_CAN_RECEIVER

#include "PylontechCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <algorithm>
#include <driver/twai.h>
#include <ctime>

//#define PYLONTECH_DUMMY

bool PylontechCanReceiver::init()
{
    return _initialized = BatteryCanReceiver::init("Pylontech");
}

void PylontechCanReceiver::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x351: {
            _stats->chargeVoltage = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->chargeCurrentLimit = this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1);
            _stats->setDischargeCurrentLimit(this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1), millis());
            _stats->dischargeVoltageLimit = this->scaleValue(this->readUnsignedInt16(rx_message.data + 6), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s chargeVoltage: %f chargeCurrentLimitation: %f dischargeCurrentLimitation: %f dischargeVoltageLimitation: %f\r\n", _providerName,
                        _stats->chargeVoltage,
                        _stats->chargeCurrentLimit,
                        _stats->getDischargeCurrentLimit(),
                        _stats->dischargeVoltageLimit);
            }
            break;
        }

        case 0x355: {
            _stats->setSoC(static_cast<float>(this->readUnsignedInt16(rx_message.data)), 0/*precision*/, millis());
            _stats->stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);

            if (_verboseLogging) {
                MessageOutput.printf("%s soc: %.1f soh: %d\r\n", _providerName, _stats->getSoC(), _stats->stateOfHealth);
            }
            break;
        }

        case 0x356: {
            _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.01), millis());
            _stats->setCurrent(this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1), 1/*precision*/, millis());
            _stats->temperature = this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s voltage: %f current: %f temperature: %f\r\n", _providerName,
                        _stats->getVoltage(), _stats->chargeCurrent(), _stats->temperature);
            }
            break;
        }

        case 0x359: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->Alarm.overCurrentDischarge = this->getBit(alarmBits, 7);
            _stats->Alarm.underTemperature = this->getBit(alarmBits, 4);
            _stats->Alarm.overTemperature = this->getBit(alarmBits, 3);
            _stats->Alarm.underVoltage = this->getBit(alarmBits, 2);
            _stats->Alarm.overVoltage= this->getBit(alarmBits, 1);

            alarmBits = rx_message.data[1];
            _stats->Alarm.bmsInternal = this->getBit(alarmBits, 3);
            _stats->Alarm.overCurrentCharge = this->getBit(alarmBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("%s Alarms: %d %d %d %d %d %d %d\r\n", _providerName,
                        _stats->Alarm.overCurrentDischarge,
                        _stats->Alarm.underTemperature,
                        _stats->Alarm.overTemperature,
                        _stats->Alarm.underVoltage,
                        _stats->Alarm.overVoltage,
                        _stats->Alarm.bmsInternal,
                        _stats->Alarm.overCurrentCharge);
            }

            uint16_t warningBits = rx_message.data[2];
            _stats->Warning.highCurrentDischarge = this->getBit(warningBits, 7);
            _stats->Warning.lowTemperature = this->getBit(warningBits, 4);
            _stats->Warning.highTemperature = this->getBit(warningBits, 3);
            _stats->Warning.lowVoltage = this->getBit(warningBits, 2);
            _stats->Warning.highVoltage = this->getBit(warningBits, 1);

            warningBits = rx_message.data[3];
            _stats->Warning.bmsInternal= this->getBit(warningBits, 3);
            _stats->Warning.highCurrentCharge = this->getBit(warningBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("%s Warnings: %d %d %d %d %d %d %d\r\n", _providerName,
                        _stats->Warning.highCurrentDischarge,
                        _stats->Warning.lowTemperature,
                        _stats->Warning.highTemperature,
                        _stats->Warning.lowVoltage,
                        _stats->Warning.highVoltage,
                        _stats->Warning.bmsInternal,
                        _stats->Warning.highCurrentCharge);
            }

            _stats->moduleCount = rx_message.data[4];
            if (_verboseLogging) {
                MessageOutput.printf("%s Modules: %d\r\n", _providerName, _stats->moduleCount);
            }

            break;
        }

        case 0x35E: {
            String manufacturer(reinterpret_cast<char*>(rx_message.data),
                    std::min(5, static_cast<int>(rx_message.data_length_code)));

            if (manufacturer.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s Manufacturer: %s\r\n", _providerName, manufacturer.c_str());
            }

            _stats->setManufacturer(manufacturer);
            break;
        }

        case 0x35C: {
            uint16_t chargeStatusBits = rx_message.data[0];
            _stats->chargeEnabled = this->getBit(chargeStatusBits, 7);
            _stats->dischargeEnabled = this->getBit(chargeStatusBits, 6);
            _stats->chargeImmediately = this->getBit(chargeStatusBits, 5);

            if (_verboseLogging) {
                MessageOutput.printf("%s chargeStatusBits: %d %d %d\r\n", _providerName,
                    _stats->chargeEnabled,
                    _stats->dischargeEnabled,
                    _stats->chargeImmediately);
            }

            break;
        }

        default:
            return; // do not update last update timestamp
    }

    _stats->setLastUpdate(millis());
}

#ifdef PYLONTECH_DUMMY
void PylontechCanReceiver::dummyData()
{
    static uint32_t lastUpdate = millis();
    static uint8_t issues = 0;

    if (millis() < (lastUpdate + 5 * 1000)) { return; }

    lastUpdate = millis();
    _stats->setLastUpdate(lastUpdate);

    auto dummyFloat = [](int offset) -> float {
        return offset + (static_cast<float>((lastUpdate + offset) % 10) / 10);
    };

    _stats->setManufacturer("Pylontech US3000C");
    _stats->setSoC(42, 0, millis());
    _stats->chargeVoltage = dummyFloat(50);
    _stats->chargeCurrentLimit = dummyFloat(33);
    _stats->setDischargeCurrentLimit(dummyFloat(12), millis());
    _stats->dischargeVoltageLimit = dummyFloat(46);
    _stats->stateOfHealth = 99;
    _stats->setVoltage(48.67, millis());
    _stats->setCurrent(dummyFloat(-1), 1/*precision*/, millis());
    _stats->temperature = dummyFloat(20);

    _stats->chargeEnabled = true;
    _stats->dischargeEnabled = true;
    _stats->chargeImmediately = false;
    _stats->moduleCount = 1;

    _stats->Warning.highCurrentDischarge = false;
    _stats->Warning.highCurrentCharge = false;
    _stats->Warning.lowTemperature = false;
    _stats->Warning.highTemperature = false;
    _stats->Warning.lowVoltage = false;
    _stats->Warning.highVoltage = false;
    _stats->Warning.bmsInternal = false;

    _stats->Alarm.overCurrentDischarge = false;
    _stats->Alarm.overCurrentCharge = false;
    _stats->Alarm.underTemperature = false;
    _stats->Alarm.overTemperature = false;
    _stats->Alarm.underVoltage = false;
    _stats->Alarm.overVoltage = false;
    _stats->Alarm.bmsInternal = false;

    if (issues == 1 || issues == 3) {
        _stats->Warning.highCurrentDischarge = true;
        _stats->Warning.highCurrentCharge = true;
        _stats->Warning.lowTemperature = true;
        _stats->Warning.highTemperature = true;
        _stats->Warning.lowVoltage = true;
        _stats->Warning.highVoltage = true;
        _stats->Warning.bmsInternal = true;
    }

    if (issues == 2 || issues == 3) {
        _stats->Alarm.overCurrentDischarge = true;
        _stats->Alarm.overCurrentCharge = true;
        _stats->Alarm.underTemperature = true;
        _stats->Alarm.overTemperature = true;
        _stats->Alarm.underVoltage = true;
        _stats->Alarm.overVoltage = true;
        _stats->Alarm.bmsInternal = true;
    }

    if (issues == 4) {
        _stats->Warning.highCurrentCharge = true;
        _stats->Warning.lowTemperature = true;
        _stats->Alarm.underVoltage = true;
        _stats->dischargeEnabled = false;
        _stats->chargeImmediately = true;
    }

    issues = (issues + 1) % 5;
}
#endif

#endif
