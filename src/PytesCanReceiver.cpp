// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYTES_CAN_RECEIVER

#include "PytesCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>

bool PytesCanReceiver::init()
{
    return _initialized = BatteryCanReceiver::init("Pytes");
}

void PytesCanReceiver::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x351:
        case 0x400:
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

        case 0x355:  // Victron protocol: SOC/SOH
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data)), 0/*precision*/, millis());
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);

            if (_verboseLogging) {
                MessageOutput.printf("%s soc: %.1f soh: %d\r\n", _providerName, _stats->getSoC(), _stats->_stateOfHealth);
            }
            break;

        case 0x356:
        case 0x405:
            _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.01), millis());
            _stats->setCurrent(this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1), 1/*precision*/, millis());
            _stats->_temperature = this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s voltage: %f current: %f temperature: %f\r\n", _providerName,
                        _stats->getVoltage(), _stats->getChargeCurrent(), _stats->_temperature);
            }
            break;

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
            }
            break;

        case 0x35E:
        case 0x40A: {
            String manufacturer(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (manufacturer.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s Manufacturer: %s\r\n", _providerName, manufacturer.c_str());
            }

            _stats->setManufacturer(std::move(manufacturer));
            }
            break;

        case 0x35F: { // Victron protocol: BatteryInfo
            auto fwVersionPart1 = String(this->readUnsignedInt8(rx_message.data + 2));
            auto fwVersionPart2 = String(this->readUnsignedInt8(rx_message.data + 3));
            _stats->_fwversion = "v" + fwVersionPart1 + "." + fwVersionPart2;

            _stats->_availableCapacity = this->readUnsignedInt16(rx_message.data + 4);

            if (_verboseLogging) {
                MessageOutput.printf("%s fwversion: %s availableCapacity: %f Ah\r\n", _providerName,
                        _stats->_fwversion.c_str(), _stats->_availableCapacity);
            }
            }
            break;

        case 0x372:  // Victron protocol: BankInfo
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

        case 0x373:  // Victron protocol: CellInfo
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

        case 0x374: { // Victron protocol: Battery/Cell name (string) with "Lowest Cell Voltage"
            String cellMinVoltageName(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (cellMinVoltageName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMinVoltageName: %s\r\n", _providerName, cellMinVoltageName.c_str());
            }

            _stats->_cellMinVoltageName = cellMinVoltageName;
            }
            break;

        case 0x375: { // Victron protocol: Battery/Cell name (string) with "Highest Cell Voltage"
            String cellMaxVoltageName(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (cellMaxVoltageName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMaxVoltageName: %s\r\n", _providerName, cellMaxVoltageName.c_str());
            }

            _stats->_cellMaxVoltageName = cellMaxVoltageName;
            }
            break;

        case 0x376: { // Victron Protocol: Battery/Cell name (string) with "Minimum Cell Temperature"
            String cellMinTemperatureName(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (cellMinTemperatureName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMinTemperatureName: %s\r\n", _providerName, cellMinTemperatureName.c_str());
            }

            _stats->_cellMinTemperatureName = cellMinTemperatureName;
            }
            break;

        case 0x377: { // Victron Protocol: Battery/Cell name (string) with "Maximum Cell Temperature"
            String cellMaxTemperatureName(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (cellMaxTemperatureName.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s cellMaxTemperatureName: %s\r\n", _providerName, cellMaxTemperatureName.c_str());
            }

            _stats->_cellMaxTemperatureName = cellMaxTemperatureName;
            }
            break;

        case 0x378:
        case 0x41e:  // History: Charged / Discharged Energy
            _stats->_chargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data), 0.1);
            _stats->_dischargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data + 4), 0.1);

            if (_verboseLogging) {
                MessageOutput.printf("%s chargedEnergy: %f dischargedEnergy: %f\r\n", _providerName,
                        _stats->_chargedEnergy, _stats->_dischargedEnergy);
            }
            break;

        case 0x379: // BatterySize: Installed Ah
            _stats->_totalCapacity = this->readUnsignedInt16(rx_message.data);

            if (_verboseLogging) {
                MessageOutput.printf("%s totalCapacity: %f Ah\r\n", _providerName, _stats->_totalCapacity);
            }
            break;

        case 0x380: { // Serialnumber - part 1
            String snPart1(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (snPart1.isEmpty() || !isgraph(snPart1.charAt(0))) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s snPart1: %s\r\n", _providerName, snPart1.c_str());
            }

            _stats->_serialPart1 = snPart1;
            _stats->updateSerial();
            }
            break;

        case 0x381: {  // Serialnumber - part 2
            String snPart2(reinterpret_cast<char*>(rx_message.data), rx_message.data_length_code);

            if (snPart2.isEmpty() || !isgraph(snPart2.charAt(0))) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("%s snPart2: %s\r\n", _providerName, snPart2.c_str());
            }

            _stats->_serialPart2 = snPart2;
            _stats->updateSerial();
            }
            break;

        case 0x401: { // Pytes protocol: Highest/Lowest Cell Voltage
            _stats->_cellMaxMilliVolt = this->readUnsignedInt16(rx_message.data);
            _stats->_cellMinMilliVolt = this->readUnsignedInt16(rx_message.data + 2);
            char maxName[8];
            char minName[8];
            snprintf(maxName, sizeof(maxName), "%04x", this->readUnsignedInt16(rx_message.data + 4));
            snprintf(minName, sizeof(minName), "%04x", this->readUnsignedInt16(rx_message.data + 6));
            _stats->_cellMaxVoltageName = String(maxName);
            _stats->_cellMinVoltageName = String(minName);

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] lowestCellMilliVolt: %d highestCellMilliVolt: %d cellMinVoltageName: %s cellMaxVoltageName: %s\r\n",
                        _stats->_cellMinMilliVolt, _stats->_cellMaxMilliVolt, minName, maxName);
            }
            }
            break;

        case 0x402: { // Pytes protocol: Highest/Lowest Cell Temperature
            _stats->_cellMaxTemperature = static_cast<float>(this->readUnsignedInt16(rx_message.data)) / 10.0;
            _stats->_cellMinTemperature = static_cast<float>(this->readUnsignedInt16(rx_message.data + 2)) / 10.0;
            char maxName[8];
            char minName[8];
            snprintf(maxName, sizeof(maxName), "%04x", this->readUnsignedInt16(rx_message.data + 4));
            snprintf(minName, sizeof(minName), "%04x", this->readUnsignedInt16(rx_message.data + 6));
            _stats->_cellMaxTemperatureName = String(maxName);
            _stats->_cellMinTemperatureName = String(minName);

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] minimumCellTemperature: %f maximumCellTemperature: %f cellMinTemperatureName: %s cellMaxTemperatureName: %s\r\n",
                        _stats->_cellMinTemperature, _stats->_cellMaxTemperature, minName, maxName);
            }
            }
            break;

        case 0x403: { // Pytes protocol: Alarms and Warnings (part 1)
            uint32_t alarmBits1 = this->readUnsignedInt32(rx_message.data);
            uint32_t alarmBits2 = this->readUnsignedInt32(rx_message.data + 4);
            uint32_t mergedBits = alarmBits1 | alarmBits2;

            bool overVoltage = this->getBit(mergedBits, 0);
            bool highVoltage = this->getBit(mergedBits, 1);
            // bool normalVoltage = this->getBit(mergedBits, 2);
            bool lowVoltage = this->getBit(mergedBits, 3);
            bool underVoltage = this->getBit(mergedBits, 4);
            // bool chargeHigh = this->getBit(mergedBits, 5);
            // bool chargeMiddle = this->getBit(mergedBits, 6);
            // bool chargeLow = this->getBit(mergedBits, 7);
            bool overTemp = this->getBit(mergedBits, 8);
            bool highTemp = this->getBit(mergedBits, 9);
            // bool normalTemp = this->getBit(mergedBits, 10);
            bool lowTemp = this->getBit(mergedBits, 11);
            bool underTemp = this->getBit(mergedBits, 12);
            bool overCurrentDischarge = this->getBit(mergedBits, 17) || this->getBit(mergedBits, 18);
            bool overCurrentCharge = this->getBit(mergedBits, 19) || this->getBit(mergedBits, 20);
            bool highCurrentDischarge = this->getBit(mergedBits, 21);
            bool highCurrentCharge = this->getBit(mergedBits, 22);
            // bool stateIdle = this->getBit(mergedBits, 25);
            bool stateCharging = this->getBit(mergedBits, 26);
            bool stateDischarging = this->getBit(mergedBits, 27);

            _stats->Alarm.overVoltage = overVoltage;
            _stats->Alarm.underVoltage = underVoltage;
            _stats->Alarm.overTemperature = stateDischarging && overTemp;
            _stats->Alarm.underTemperature = stateDischarging && underTemp;
            _stats->Alarm.overTemperatureCharge = stateCharging && overTemp;
            _stats->Alarm.underTemperatureCharge = stateCharging && underTemp;

            _stats->Alarm.overCurrentDischarge = overCurrentDischarge;
            _stats->Alarm.overCurrentCharge = overCurrentCharge;

            _stats->Warning.highVoltage = highVoltage;
            _stats->Warning.lowVoltage = lowVoltage;
            _stats->Warning.highTemperature = stateDischarging && highTemp;
            _stats->Warning.lowTemperature = stateDischarging && lowTemp;
            _stats->Warning.highTemperatureCharge = stateCharging && highTemp;
            _stats->Warning.lowTemperatureCharge = stateCharging && lowTemp;

            _stats->Warning.highCurrentDischarge = highCurrentDischarge;
            _stats->Warning.highCurrentCharge = highCurrentCharge;

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] Warnings: %08x %08x\r\n",
                        alarmBits1, alarmBits2);
            }
            break;
            }

        case 0x404: { // Pytes protocol: SOC/SOH
            // soc isn't used here since it is generated with higher
            // precision in message 0x0409 below.
            uint8_t soc = static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data));
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_chargeCycles = this->readUnsignedInt16(rx_message.data + 6);

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] soc: %d soh: %d cycles: %d\r\n",
                        soc, _stats->_stateOfHealth, _stats->_chargeCycles);
            }
            }
            break;

        case 0x406: { // Pytes protocol: alarms (part 2)
            uint32_t alarmBits = this->readUnsignedInt32(rx_message.data);
            _stats->Alarm.bmsInternal = this->getBit(alarmBits, 15);

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] internalFailure: %d (bits: %08x)\r\n",
                        _stats->Alarm.bmsInternal, alarmBits);
            }
            break;
            }

        case 0x408: { // Pytes protocol: charge status (similar to Pylontech msg 0x35e)
            bool chargeEnabled = rx_message.data[0];
            bool dischargeEnabled = rx_message.data[1];
            bool chargeImmediately = rx_message.data[2];

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] chargeEnabled: %d dischargeEnabled: %d chargeImmediately: %d\r\n",
                    chargeEnabled, dischargeEnabled, chargeImmediately);
            }
            break;
            }

        case 0x409: { // Pytes protocol: full mAh / remaining mAh
            _stats->_totalCapacity = static_cast<float>(this->readUnsignedInt32(rx_message.data)) / 1000.0;
            _stats->_availableCapacity = static_cast<float>(this->readUnsignedInt32(rx_message.data + 4)) / 1000.0;
            float soc = 100.0 * static_cast<float>(_stats->_availableCapacity) / static_cast<float>(_stats->_totalCapacity);
            _stats->setSoC(soc, 2/*precision*/, millis());

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] soc: %f totalCapacity: %f Ah availableCapacity: %f Ah \r\n",
                        soc, _stats->_totalCapacity, _stats->_availableCapacity);
            }
            }
            break;

        case 0x40b: // Pytes protocol: online / offline module count
            _stats->_moduleCountOnline = this->readUnsignedInt8(rx_message.data + 6);
            _stats->_moduleCountOffline = this->readUnsignedInt8(rx_message.data + 7);

            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] moduleCountOnline: %d moduleCountOffline: %d\r\n",
                        _stats->_moduleCountOnline, _stats->_moduleCountOffline);
            }
            break;

        case 0x40d: // Pytes protocol: balancing info
            _stats->_balance = this->readUnsignedInt16(rx_message.data + 4);
            if (_verboseLogging) {
                MessageOutput.printf("[Pytes] balance: %d\r\n", _stats->_balance);
            }
            break;

        default:
            return; // do not update last update timestamp
    }

    _stats->setLastUpdate(millis());
}

#endif
