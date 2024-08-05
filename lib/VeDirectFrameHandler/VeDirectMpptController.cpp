/* VeDirectMpptController.cpp
 *
 *
 * 2020.08.20 - 0.0 - ???
 * 2024.03.18 - 0.1 - add of: - temperature from "Smart Battery Sense" connected over VE.Smart network
 * 					  		  - temperature from internal MPPT sensor
 * 					  		  - "total DC input power" from MPPT's connected over VE.Smart network
 */

#include <Arduino.h>
#include "VeDirectMpptController.h"

static constexpr char TAG[] = " Hex Data: ";

//#define PROCESS_NETWORK_STATE

void VeDirectMpptController::init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging, uint8_t hwSerialPort)
{
	VeDirectFrameHandler::init("MPPT", rx, tx, msgOut, verboseLogging, hwSerialPort);
}

bool VeDirectMpptController::processTextDataDerived(std::string const& name, std::string const& value)
{
	if (name == "IL") {
		_tmpFrame.loadCurrent_IL_mA = atol(value.c_str());
		return true;
	}
	if (name == "LOAD") {
		_tmpFrame.loadOutputState_LOAD = (value == "ON");
		return true;
	}
	if (name == "CS") {
		_tmpFrame.currentState_CS = atoi(value.c_str());
		return true;
	}
	if (name == "ERR") {
		_tmpFrame.errorCode_ERR = atoi(value.c_str());
		return true;
	}
	if (name == "OR") {
		_tmpFrame.offReason_OR = strtol(value.c_str(), nullptr, 0);
		return true;
	}
	if (name == "MPPT") {
		_tmpFrame.stateOfTracker_MPPT = atoi(value.c_str());
		return true;
	}
	if (name == "HSDS") {
		_tmpFrame.daySequenceNr_HSDS = atoi(value.c_str());
		return true;
	}
	if (name == "VPV") {
		_tmpFrame.panelVoltage_VPV_mV = atol(value.c_str());
		return true;
	}
	if (name == "PPV") {
		_tmpFrame.panelPower_PPV_W = atoi(value.c_str());
		return true;
	}
	if (name == "H19") {
		_tmpFrame.yieldTotal_H19_Wh = atol(value.c_str()) * 10;
		return true;
	}
	if (name == "H20") {
		_tmpFrame.yieldToday_H20_Wh = atol(value.c_str()) * 10;
		return true;
	}
	if (name == "H21") {
		_tmpFrame.maxPowerToday_H21_W = atoi(value.c_str());
		return true;
	}
	if (name == "H22") {
		_tmpFrame.yieldYesterday_H22_Wh = atol(value.c_str()) * 10;
		return true;
	}
	if (name == "H23") {
		_tmpFrame.maxPowerYesterday_H23_W = atoi(value.c_str());
		return true;
	}

	return false;
}

/*
 *  frameValidEvent
 *  This function is called at the end of the received frame.
 */
void VeDirectMpptController::frameValidEvent() {
	// power into the battery, (+) means charging, (-) means discharging
	_tmpFrame.batteryOutputPower_W = static_cast<int16_t>((_tmpFrame.batteryVoltage_V_mV / 1000.0f) * (_tmpFrame.batteryCurrent_I_mA / 1000.0f));

	// calculation of the panel current
	if ((_tmpFrame.panelVoltage_VPV_mV > 0) && (_tmpFrame.panelPower_PPV_W >= 1)) {
		_tmpFrame.panelCurrent_mA = static_cast<uint32_t>(_tmpFrame.panelPower_PPV_W * 1000000.0f / _tmpFrame.panelVoltage_VPV_mV);
	} else {
		_tmpFrame.panelCurrent_mA = 0;
	}

	// calculation of the MPPT efficiency
	float totalPower_W = (_tmpFrame.loadCurrent_IL_mA / 1000.0f + _tmpFrame.batteryCurrent_I_mA / 1000.0f) * _tmpFrame.batteryVoltage_V_mV /1000.0f;
	if (_tmpFrame.panelPower_PPV_W > 0) {
		_efficiency.addNumber(totalPower_W * 100.0f / _tmpFrame.panelPower_PPV_W);
		_tmpFrame.mpptEfficiency_Percent = _efficiency.getAverage();
	} else {
		_tmpFrame.mpptEfficiency_Percent = 0.0f;
	}

	if (!_canSend) { return; }

	// Copy from the "VE.Direct Protocol" documentation
	// For firmware version v1.52 and below, when no VE.Direct queries are sent to the device, the
	// charger periodically sends human readable (TEXT) data to the serial port. For firmware
	// versions v1.53 and above, the charger always periodically sends TEXT data to the serial port.
	// --> We just use hex commands for firmware >= 1.53 to keep text messages alive
	if (_tmpFrame.getFwVersionAsInteger() < 153) { return; }

	// It seems some commands get lost if we send to fast the next command.
	// Maybe we produce an overflow on the MPPT receive buffer or we have to wait for the MPPT answer
	// before we can send the next command.
	// Now we send only one command after every text-mode-frame.
	// We need a better solution if we add more commands
	// Don't worry about the NetworkTotalDcInputPower. We get anyway asynchronous messages on every value change
	VeDirectHexRegister thisRegister = _slotRegister[_slotNr++];
	if (_slotNr >= _slotRegister.size())
		_slotNr = 0;

    // do not request if device has no Load output
    if ((thisRegister == VeDirectHexRegister::LoadOutputVoltage ||
         thisRegister == VeDirectHexRegister::LoadCurrent ||
         thisRegister == VeDirectHexRegister::LoadOutputState ||
         thisRegister == VeDirectHexRegister::LoadOutputControl) &&
        (!(_tmpFrame.Capabilities.second & (1<<0)))) return;

  	sendHexCommand(VeDirectHexCommand::GET, thisRegister);
}

void VeDirectMpptController::loop()
{
	VeDirectFrameHandler::loop();

	auto resetTimestamp = [this](auto& pair) {
		if (pair.first > 0 && (millis() - pair.first) > (30 * 1000)) {
			pair.first = 0;
		}
	};

	resetTimestamp(_tmpFrame.Capabilities);
	resetTimestamp(_tmpFrame.ChargerVoltage);
	resetTimestamp(_tmpFrame.ChargerCurrent);
	resetTimestamp(_tmpFrame.ChargerMaximumCurrent);
	resetTimestamp(_tmpFrame.VoltageSettingsRange);
    if (_tmpFrame.Capabilities.second & 1) {
    	resetTimestamp(_tmpFrame.LoadOutputState);
	    resetTimestamp(_tmpFrame.LoadOutputControl);
	    resetTimestamp(_tmpFrame.LoadOutputVoltage);
	    resetTimestamp(_tmpFrame.LoadOutputControl);
    }
	resetTimestamp(_tmpFrame.BatteryType);
	resetTimestamp(_tmpFrame.BatteryMaximumCurrent);
	resetTimestamp(_tmpFrame.MpptTemperatureMilliCelsius);
	resetTimestamp(_tmpFrame.SmartBatterySenseTemperatureMilliCelsius);
	resetTimestamp(_tmpFrame.NetworkTotalDcInputPowerMilliWatts);
	resetTimestamp(_tmpFrame.BatteryFloatMilliVolt);
	resetTimestamp(_tmpFrame.BatteryAbsorptionMilliVolt);
	resetTimestamp(_tmpFrame.PanelPowerMilliWatt);
	resetTimestamp(_tmpFrame.PanelVoltageMilliVolt);
	resetTimestamp(_tmpFrame.PanelCurrent);
	resetTimestamp(_tmpFrame.BatteryVoltageSetting);

#ifdef PROCESS_NETWORK_STATE
	resetTimestamp(_tmpFrame.NetworkInfo);
	resetTimestamp(_tmpFrame.NetworkMode);
	resetTimestamp(_tmpFrame.NetworkStatus);
#endif // PROCESS_NETWORK_STATE
}


/*
 * hexDataHandler()
 * analyze the content of VE.Direct hex messages
 * handel's the received hex data from the MPPT
 */
bool VeDirectMpptController::hexDataHandler(VeDirectHexData const &data) {
	if (data.rsp != VeDirectHexResponse::GET &&
			data.rsp != VeDirectHexResponse::ASYNC) { return false; }

	auto regLog = static_cast<uint16_t>(data.addr);

boolean forceLogging = false;

	switch (data.addr) {
        case VeDirectHexRegister::BatteryVoltageSetting:
			_tmpFrame.BatteryVoltageSetting = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sBattery Voltage Setting (0x%04X): (%u) %s\r\n",
                    _logId, TAG, regLog,
                    data.value,
                    _tmpFrame.getBatteryVoltageSettingAsString().data());
            }
            return true;

        case VeDirectHexRegister::Capabilities:
			_tmpFrame.Capabilities = { millis(), static_cast<uint32_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sCapabilities (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
                for (uint8_t bit=0; bit <=21; bit++)
    				_msgOut->printf("%s %s: %s\r\n", _logId,
                        _tmpFrame.getCapabilitiesAsString(bit).data(), (_tmpFrame.Capabilities.second & (1<<bit))?"yes":"no");
                for (uint8_t bit=25; bit <=27; bit++)
    				_msgOut->printf("%s %s: %s\r\n", _logId,
                        _tmpFrame.getCapabilitiesAsString(bit).data(), (_tmpFrame.Capabilities.second & (1<<bit))?"yes":"no");
			}
			return true;

		case VeDirectHexRegister::ChargeControllerTemperature:
			_tmpFrame.MpptTemperatureMilliCelsius = { millis(), static_cast<int32_t>(data.value) * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sMPPT Temperature (0x%04X): %.2f°C\r\n", _logId, TAG, regLog,
						_tmpFrame.MpptTemperatureMilliCelsius.second / 1000.0);
			}
			return true;

		case VeDirectHexRegister::SmartBatterySenseTemperature:
			if ((data.value &0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sSmart Battery Sense Temperature is not available\r\n", _logId, TAG);
				}
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.SmartBatterySenseTemperatureMilliCelsius = { millis(), static_cast<int32_t>(data.value) * 10 - 272150 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sSmart Battery Sense Temperature (0x%04X): %.2f°C\r\n", _logId, TAG, regLog,
						_tmpFrame.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0);
			}
			return true;

		case VeDirectHexRegister::LoadOutputState:
			_tmpFrame.LoadOutputState = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sLoad output state (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

		case VeDirectHexRegister::LoadOutputControl:
			_tmpFrame.LoadOutputControl = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sLoad output control (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

        case VeDirectHexRegister::LoadOutputVoltage:
			if ((data.value & 0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sLoad output voltage is not available\r\n", _logId, TAG);
				}
				_tmpFrame.LoadOutputVoltage = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.LoadOutputVoltage = { millis(), data.value * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sLoad output voltage (0x%04X): %.2fV\r\n", _logId, TAG, regLog,
						_tmpFrame.LoadOutputVoltage.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::LoadCurrent:
			if ((data.value & 0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sLoad current is not available\r\n", _logId, TAG);
				}
				_tmpFrame.LoadCurrent = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.LoadCurrent = { millis(), data.value * 100 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sLoad current (0x%04X): %.1fA\r\n", _logId, TAG, regLog,
						_tmpFrame.LoadCurrent.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::ChargerVoltage:
			if (data.value == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sCharger voltage is not available\r\n", _logId, TAG);
				}
				_tmpFrame.ChargerVoltage = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.ChargerVoltage = { millis(), data.value * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sCharger voltage (0x%04X): %.2fV\r\n", _logId, TAG, regLog,
						_tmpFrame.ChargerVoltage.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::ChargerCurrent:
			if ((data.value & 0xFFFF)== 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sCharger current is not available\r\n", _logId, TAG);
				}
				_tmpFrame.ChargerCurrent = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.ChargerCurrent = { millis(), data.value * 100 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sCharger current (0x%04X): %.1fA\r\n", _logId, TAG, regLog,
						_tmpFrame.ChargerCurrent.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::ChargerMaximumCurrent:
			if ((data.value & 0xffff) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sCharger maximum current is not available\r\n", _logId, TAG);
				}
				_tmpFrame.ChargerMaximumCurrent = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.ChargerMaximumCurrent = { millis(), data.value * 100 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sCharger maximum current (0x%04X): %.1fA\r\n", _logId, TAG, regLog,
						_tmpFrame.ChargerMaximumCurrent.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::VoltageSettingsRange:
			_tmpFrame.VoltageSettingsRange = { millis(), data.value };

			if (_verboseLogging) {
				_msgOut->printf("%s%sVoltage Settings Range (0x%04X): min %uV, max %uV\r\n", _logId, TAG, regLog,
						_tmpFrame.VoltageSettingsRange.second & 0xFF, (_tmpFrame.VoltageSettingsRange.second >> 8) & 0xFF);
			}
			return true;

		case VeDirectHexRegister::NetworkTotalDcInputPower:
			if (data.value == 0xFFFFFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sNetwork total DC power value indicates non-networked controller\r\n", _logId, TAG);
				}
				_tmpFrame.NetworkTotalDcInputPowerMilliWatts = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.NetworkTotalDcInputPowerMilliWatts = { millis(), data.value * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sNetwork Total DC Power (0x%04X): %.2fW\r\n", _logId, TAG, regLog,
						_tmpFrame.NetworkTotalDcInputPowerMilliWatts.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::BatteryMaximumCurrent:
			if ((data.value &0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sBattery maximum current is not available\r\n", _logId, TAG);
				}
				_tmpFrame.BatteryMaximumCurrent = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.BatteryMaximumCurrent = { millis(), data.value * 100 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sBattery maximum current (0x%04X): %.1fA\r\n", _logId, TAG, regLog,
						_tmpFrame.BatteryMaximumCurrent.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::BatteryAbsorptionVoltage:
			if ((data.value & 0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sBattery absorption voltage is not available\r\n", _logId, TAG);
				}
				_tmpFrame.BatteryAbsorptionMilliVolt = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.BatteryAbsorptionMilliVolt = { millis(), static_cast<uint32_t>(data.value) * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sMPPT Absorption Voltage (0x%04X): %.2fV\r\n",
						_logId, TAG, regLog,
						_tmpFrame.BatteryAbsorptionMilliVolt.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::BatteryFloatVoltage:
			if ((data.value & 0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging) {
					_msgOut->printf("%s%sBattery float voltage is not available\r\n", _logId, TAG);
				}
				_tmpFrame.BatteryFloatMilliVolt = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.BatteryFloatMilliVolt = { millis(), static_cast<uint32_t>(data.value) * 10 };

			if (_verboseLogging) {
				_msgOut->printf("%s%sMPPT Float Voltage (0x%04X): %.2fV\r\n",
						_logId, TAG, regLog,
						_tmpFrame.BatteryFloatMilliVolt.second / 1000.0);
			}
			return true;

		case VeDirectHexRegister::BatteryType:
			_tmpFrame.BatteryType = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sBattery type (0x%04X): %s\r\n", _logId, TAG, regLog, _tmpFrame.getBatteryTypeAsString().data());
			}
			return true;

		case VeDirectHexRegister::DeviceMode:
			_tmpFrame.DeviceMode = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sDevice Mode (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

		case VeDirectHexRegister::DeviceState:
			_tmpFrame.DeviceState = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sDevice State (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

		case VeDirectHexRegister::RemoteControlUsed:
			_tmpFrame.RemoteControlUsed = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging) {
				_msgOut->printf("%s%sRemote Control Used (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

#ifdef PROCESS_NETWORK_STATE
		case VeDirectHexRegister::NetworkInfo:
			_tmpFrame.NetworkInfo = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sNetwork Info (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

		case VeDirectHexRegister::NetworkMode:
			_tmpFrame.NetworkMode = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sNetwork Mode (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;

		case VeDirectHexRegister::NetworkStatus:
			_tmpFrame.NetworkStatus = { millis(), static_cast<uint8_t>(data.value) };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sNetwork Status (0x%04X): 0x%X\r\n", _logId, TAG, regLog, data.value);
			}
			return true;
#endif // PROCESS_NETWORK_STATE

		case VeDirectHexRegister::PanelPower:
			if (data.value == 0xFFFFFFFF || data.flags != 0) {
				if (_verboseLogging || forceLogging) {
					_msgOut->printf("%s%sPanel Power is not available\r\n", _logId, TAG);
				}
				_tmpFrame.PanelPowerMilliWatt = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}
            _tmpFrame.PanelPowerMilliWatt = { millis(), static_cast<uint32_t>(data.value) * 10 };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sPanel Power (0x%04X): %.2fW\r\n",
						_logId, TAG, regLog,
						_tmpFrame.PanelPowerMilliWatt.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::PanelVoltage:
			if (data.value == 0xFFFF || data.flags != 0) {
				if (_verboseLogging || forceLogging) {
					_msgOut->printf("%s%sPanel Voltage is not available\r\n", _logId, TAG);
				}
				_tmpFrame.PanelVoltageMilliVolt = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}
            _tmpFrame.PanelVoltageMilliVolt = { millis(), static_cast<uint32_t>(data.value) * 10 };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sPanel voltage (0x%04X): %.2fV\r\n",
						_logId, TAG, regLog,
						_tmpFrame.PanelVoltageMilliVolt.second / 1000.0);
			}
			return true;

        case VeDirectHexRegister::PanelCurrent:
			if ((data.value & 0xFFFF) == 0xFFFF || data.flags != 0) {
				if (_verboseLogging || forceLogging) {
					_msgOut->printf("%s%sPanel current is not available. Flags: (%02x) %s\r\n",
                        _logId, TAG,
                        data.flags, data.getFlagsAsString().data());
				}
				_tmpFrame.PanelCurrent = { 0, 0 };
				return true; // we know what to do with it, and we decided to ignore the value
			}

			_tmpFrame.PanelCurrent = { millis(), static_cast<uint32_t>(data.value) * 100 };

			if (_verboseLogging || forceLogging) {
				_msgOut->printf("%s%sPanel current (0x%04X): %.1fA\r\n", _logId, TAG, regLog,
						_tmpFrame.PanelCurrent.second / 1000.0);
			}
			return true;

		default:
            if (data.addr >= VeDirectHexRegister::HistoryTotal && data.addr <= VeDirectHexRegister::HistoryMPPTD30) {
    			if (_verboseLogging || forceLogging) {
	    			_msgOut->printf("%s%sHistorical Data (0x%04X)\r\n", _logId, TAG, regLog);
    			}
                return true;
            }

			return false;
	}

	return false;
}
