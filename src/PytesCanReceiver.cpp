// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYTES_CAN_RECEIVER

#include "PytesCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>

bool PytesCanReceiver::init()
{
    return _initialized = BatteryCanReceiver::init(_isCAN0 ? "[Pytes CAN0]" : "[Pytes MCP2515]");
}

void PytesCanReceiver::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x351: {
            _stats->_chargeVoltageLimit = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->_chargeCurrentLimit = this->scaleValue(this->readUnsignedInt16(rx_message.data + 2), 0.1);
            _stats->_dischargeCurrentLimit = this->scaleValue(this->readUnsignedInt16(rx_message.data + 4), 0.1);
            _stats->_dischargeVoltageLimit = this->scaleValue(this->readSignedInt16(rx_message.data + 6), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s chargeVoltageLimit: %f chargeCurrentLimit: %f dischargeCurrentLimit: %f dischargeVoltageLimit: %f\r\n", _providerName,
                        _stats->_chargeVoltageLimit, _stats->_chargeCurrentLimit,
                        _stats->_dischargeCurrentLimit, _stats->_dischargeVoltageLimit);
            }
            break;
        }

        case 0x355: {
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data)), 0/*precision*/, millis());
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);

            if (_verboseLogging) {
                MessageOutput.printf("%s soc: %.1f soh: %d\r\n", _providerName, _stats->getSoC(), _stats->_stateOfHealth);
            }
            break;
        }

        case 0x356: {
            _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.01), millis());
            _stats->_current = this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1);
            _stats->_temperature = this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s voltage: %f current: %f temperature: %f\r\n", _providerName,
                        _stats->getVoltage(), _stats->_current, _stats->_temperature);
            }
            break;
        }

        case 0x35A: { // Alarms and Warnings
            uint16_t alarmBits = rx_message.data[0];
            _stats->Alarm.overVoltage = this->getBit(alarmBits, 2);
            _stats->Alarm.underVoltage = this->getBit(alarmBits, 4);
            _stats->Alarm.overTemperature = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[1];
            _stats->Alarm.underTemperature = this->getBit(alarmBits, 0);
            _stats->Alarm.overTemperatureCharge = this->getBit(alarmBits, 2);
            _stats->Alarm.underTemperatureCharge = this->getBit(alarmBits, 4);
            _stats->Alarm.overCurrentDischarge = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[2];
            _stats->Alarm.overCurrentCharge = this->getBit(alarmBits, 0);
            _stats->Alarm.bmsInternal = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[3];
            _stats->Alarm.cellImbalance = this->getBit(alarmBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("%s Alarms: %d %d %d %d %d %d %d %d %d %d\r\n", _providerName,
                        _stats->Alarm.overVoltage,
                        _stats->Alarm.underVoltage,
                        _stats->Alarm.overTemperature,
                        _stats->Alarm.underTemperature,
                        _stats->Alarm.overTemperatureCharge,
                        _stats->Alarm.underTemperatureCharge,
                        _stats->Alarm.overCurrentDischarge,
                        _stats->Alarm.overCurrentCharge,
                        _stats->Alarm.bmsInternal,
                        _stats->Alarm.cellImbalance);
            }

            uint16_t warningBits = rx_message.data[4];
            _stats->Warning.highVoltage = this->getBit(warningBits, 2);
            _stats->Warning.lowVoltage = this->getBit(warningBits, 4);
            _stats->Warning.highTemperature = this->getBit(warningBits, 6);

            warningBits = rx_message.data[5];
            _stats->Warning.lowTemperature = this->getBit(warningBits, 0);
            _stats->Warning.highTemperatureCharge = this->getBit(warningBits, 2);
            _stats->Warning.lowTemperatureCharge = this->getBit(warningBits, 4);
            _stats->Warning.highCurrentDischarge = this->getBit(warningBits, 6);

            warningBits = rx_message.data[6];
            _stats->Warning.highCurrentCharge = this->getBit(warningBits, 0);
            _stats->Warning.bmsInternal = this->getBit(warningBits, 6);

            warningBits = rx_message.data[7];
            _stats->Warning.cellImbalance = this->getBit(warningBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("%s Warnings: %d %d %d %d %d %d %d %d %d %d\r\n", _providerName,
                        _stats->Warning.highVoltage,
                        _stats->Warning.lowVoltage,
                        _stats->Warning.highTemperature,
                        _stats->Warning.lowTemperature,
                        _stats->Warning.highTemperatureCharge,
                        _stats->Warning.lowTemperatureCharge,
                        _stats->Warning.highCurrentDischarge,
                        _stats->Warning.highCurrentCharge,
                        _stats->Warning.bmsInternal,
                        _stats->Warning.cellImbalance);
            }
            break;
        }

        case 0x35E: {
            String manufacturer(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (manufacturer.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s Manufacturer: %s\r\n", _providerName, manufacturer.c_str());
            }

            _stats->setManufacturer(std::move(manufacturer));
            break;
        }

        case 0x35F: { // BatteryInfo
            auto fwVersionPart1 = String(this->readUnsignedInt8(rx_message.data + 2));
            auto fwVersionPart2 = String(this->readUnsignedInt8(rx_message.data + 3));
            _stats->_fwversion = "v" + fwVersionPart1 + "." + fwVersionPart2;

            _stats->_availableCapacity = this->readUnsignedInt16(rx_message.data + 4);

            if (_verboseLogging) {
                MessageOutput.printf("%s fwversion: %s availableCapacity: %d Ah\r\n", _providerName,
                        _stats->_fwversion.c_str(), _stats->_availableCapacity);
            }
            break;
        }

        case 0x372: { // BankInfo
            _stats->_moduleCountOnline = this->readUnsignedInt16(rx_message.data);
            _stats->_moduleCountBlockingCharge = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_moduleCountBlockingDischarge = this->readUnsignedInt16(rx_message.data + 4);
            _stats->_moduleCountOffline = this->readUnsignedInt16(rx_message.data + 6);

            if (_verboseLogging) {
                MessageOutput.printf("%s moduleCountOnline: %d moduleCountBlockingCharge: %d moduleCountBlockingDischarge: %d moduleCountOffline: %d\r\n", _providerName,
                        _stats->_moduleCountOnline, _stats->_moduleCountBlockingCharge,
                        _stats->_moduleCountBlockingDischarge, _stats->_moduleCountOffline);
            }
            break;
        }

        case 0x373: { // CellInfo
            _stats->_cellMinMilliVolt = this->readUnsignedInt16(rx_message.data);
            _stats->_cellMaxMilliVolt = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_cellMinTemperature = this->readUnsignedInt16(rx_message.data + 4) - 273;
            _stats->_cellMaxTemperature = this->readUnsignedInt16(rx_message.data + 6) - 273;

            if (_verboseLogging) {
                MessageOutput.printf("%s lowestCellMilliVolt: %d highestCellMilliVolt: %d minimumCellTemperature: %f maximumCellTemperature: %f\r\n", _providerName,
                        _stats->_cellMinMilliVolt, _stats->_cellMaxMilliVolt,
                        _stats->_cellMinTemperature, _stats->_cellMaxTemperature);
            }
            break;
        }

        case 0x374: { // Battery/Cell name (string) with "Lowest Cell Voltage"
            String cellMinVoltageName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMinVoltageName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMinVoltageName: %s\r\n", _providerName, cellMinVoltageName.c_str());
            }

            _stats->_cellMinVoltageName = cellMinVoltageName;
            break;
        }

        case 0x375: { // Battery/Cell name (string) with "Highest Cell Voltage"
            String cellMaxVoltageName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMaxVoltageName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMaxVoltageName: %s\r\n", _providerName, cellMaxVoltageName.c_str());
            }

            _stats->_cellMaxVoltageName = cellMaxVoltageName;
            break;
        }

        case 0x376: { // Battery/Cell name (string) with "Minimum Cell Temperature"
            String cellMinTemperatureName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMinTemperatureName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMinTemperatureName: %s\r\n", _providerName, cellMinTemperatureName.c_str());
            }

            _stats->_cellMinTemperatureName = cellMinTemperatureName;
            break;
        }

        case 0x377: { // Battery/Cell name (string) with "Maximum Cell Temperature"
            String cellMaxTemperatureName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMaxTemperatureName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMaxTemperatureName: %s\r\n", _providerName, cellMaxTemperatureName.c_str());
            }

            _stats->_cellMaxTemperatureName = cellMaxTemperatureName;
            break;
        }

        case 0x378: { // History: Charged / Discharged Energy
            _stats->_chargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data), 0.1);
            _stats->_dischargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data + 4), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s chargedEnergy: %f dischargedEnergy: %f\r\n", _providerName,
                        _stats->_chargedEnergy, _stats->_dischargedEnergy);
            }
            break;
        }

        case 0x379: { // BatterySize: Installed Ah
            _stats->_totalCapacity = this->readUnsignedInt16(rx_message.data);

            if (_verboseLogging) {
                MessageOutput.printf("%s totalCapacity: %d Ah\r\n", _providerName, _stats->_totalCapacity);
            }
            break;
        }

        case 0x380: { // Serialnumber - part 1
            String snPart1(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (snPart1.isEmpty() || !isgraph(snPart1.charAt(0))) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s snPart1: %s\r\n", _providerName, snPart1.c_str());
            }

            _stats->_serialPart1 = snPart1;
            _stats->updateSerial();
            break;
        }

        case 0x381: {  // Serialnumber - part 2
            String snPart2(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (snPart2.isEmpty() || !isgraph(snPart2.charAt(0))) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s snPart2: %s\r\n", _providerName, snPart2.c_str());
            }

            _stats->_serialPart2 = snPart2;
            _stats->updateSerial();
            break;
        }

        default:
            return; // do not update last update timestamp
            break;
    }

    _stats->setLastUpdate(millis());
}

#endif
