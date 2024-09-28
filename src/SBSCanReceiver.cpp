#ifdef USE_SBS_CAN_RECEIVER

// SPDX-License-Identifier: GPL-2.0-or-later
#include "SBSCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>

bool SBSCanReceiver::init()
{
    _stats->_chargeVoltage =58.4;
    return _initialized =  BatteryCanReceiver::init("SBS");
}

void SBSCanReceiver::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x610: {
            _stats->setVoltage(this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.001), millis());
            _stats->setCurrent(this->scaleValue(this->readSignedInt16(rx_message.data + 3), 0.001), 1/*precision*/, millis());
            _stats->setSoC(static_cast<float>(this->readUnsignedInt16(rx_message.data + 6)), 1/*precision*/, millis());

            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1552 SoC: %.1f Voltage: %.3f Current: %.3f\r\n", _stats->getSoC(), _stats->getVoltage(), _stats->getChargeCurrent());
            }
            break;
        }

        case 0x630: {
            int clusterstate = rx_message.data[0];
            switch (clusterstate) {
                case 0:
                    // Battery inactive
                    _stats->_dischargeEnabled = false;
                    _stats->_chargeEnabled = false;
                    break;

                case 1:
                    // Battery Discharge mode (recuperation enabled)
                    _stats->_chargeEnabled = true;
                    _stats->_dischargeEnabled = true;
                    break;

                case 2:
                    // Battery in charge Mode (discharge with half current possible (45A))
                    _stats->_chargeEnabled = true;
                    _stats->_dischargeEnabled = true;
                    break;

                case 4:
                    // Battery Fault
                    _stats->_chargeEnabled = false;
                    _stats->_dischargeEnabled = false;
                    break;

                case 8:
                    // Battery Deepsleep
                    _stats->_chargeEnabled = false;
                    _stats->_dischargeEnabled = false;
                    break;

                default:
                    _stats->_chargeEnabled = false;
                    _stats->_dischargeEnabled = false;
                    break;
            }
            _stats->setManufacturer("SBS UniPower");

            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1584 chargeStatusBits: %d %d\r\n", _stats->_chargeEnabled, _stats->_dischargeEnabled);
            }
            break;
        }

        case 0x640: {
            _stats->setDischargeCurrentLimit((this->readSignedInt24(rx_message.data)) * 0.001, millis());
            _stats->_chargeCurrentLimit = (this->readSignedInt24(rx_message.data + 3) * 0.001);

            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1600 Currents  %.3f, %.3f\r\n", _stats->_chargeCurrentLimit, _stats->getDischargeCurrentLimit());
            }
            break;
        }

        case 0x650: {
            byte temp = rx_message.data[0];
            _stats->_temperature = (static_cast<float>(temp)-32) /1.8;

            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1616 Temp %.1f°C\r\n",_stats->_temperature);
            }
            break;
        }

        case 0x660: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->Alarm.underTemperature = this->getBit(alarmBits, 1);
            _stats->Alarm.overTemperature = this->getBit(alarmBits, 0);
            _stats->Alarm.underVoltage = this->getBit(alarmBits, 3);
            _stats->Alarm.overVoltage= this->getBit(alarmBits, 2);
            _stats->Alarm.bmsInternal= this->getBit(rx_message.data[1], 2);

            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1632 Alarms: %d %d %d %d\r\n ",
                    _stats->Alarm.underTemperature, _stats->Alarm.overTemperature,
                    _stats->Alarm.underVoltage,  _stats->Alarm.overVoltage);
            }
            break;
        }

        case 0x670: {
            uint16_t warningBits = rx_message.data[1];
            _stats->Warning.highCurrentDischarge = this->getBit(warningBits, 1);
            _stats->Warning.highCurrentCharge = this->getBit(warningBits, 0);

             if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1648 Warnings: %d %d\r\n",
                    _stats->Warning.highCurrentDischarge, _stats->Warning.highCurrentCharge);
            }
            break;
        }

        default:
            return; // do not update last update timestamp
    }

    _stats->setLastUpdate(millis());
}

#ifdef SBSCanReceiver_DUMMY
void SBSCanReceiver::dummyData()
{
    static uint32_t lastUpdate = millis();
    static uint8_t issues = 0;

    if (millis() < (lastUpdate + 5 * 1000)) { return; }

    lastUpdate = millis();
    _stats->setLastUpdate(lastUpdate);

    auto dummyFloat = [](int offset) -> float {
        return offset + (static_cast<float>((lastUpdate + offset) % 10) / 10);
    };

    _stats->setManufacturer("SBS Unipower XL");
    _stats->setSoC(42, 0/*precision*/, millis());
    _stats->_chargeVoltage = dummyFloat(50);
    _stats->_chargeCurrentLimit = dummyFloat(33);
    _stats->setDischargeCurrentLimit(dummyFloat(12), millis());
    _stats->_stateOfHealth = 99;
    _stats->setVoltage(48.67, millis());
    _stats->setCurrent(dummyFloat(-1), 1, millis());
    _stats->_temperature = dummyFloat(20);

    _stats->_chargeEnabled = true;
    _stats->_dischargeEnabled = true;

    _stats->Warning.highCurrentDischarge = false;
    _stats->Warning.highCurrentCharge = false;

    _stats->Alarm.overCurrentDischarge = false;
    _stats->Alarm.overCurrentCharge = false;
    _stats->Alarm.underVoltage = false;
    _stats->Alarm.overVoltage = false;


    if (issues == 1 || issues == 3) {
        _stats->Warning.highCurrentDischarge = true;
        _stats->Warning.highCurrentCharge = true;
    }

    if (issues == 2 || issues == 3) {
        _stats->Alarm.overCurrentDischarge = true;
        _stats->Alarm.overCurrentCharge = true;
        _stats->Alarm.underVoltage = true;
        _stats->Alarm.overVoltage = true;
    }

    if (issues == 4) {
        _stats->Warning.highCurrentCharge = true;
        _stats->Alarm.underVoltage = true;
        _stats->_dischargeEnabled = false;
    }

    issues = (issues + 1) % 5;
}
#endif

#endif
