#pragma once

#include <Arduino.h>
#include "VeDirectData.h"
#include "VeDirectFrameHandler.h"

template<typename T, size_t WINDOW_SIZE>
class MovingAverage {
public:
    MovingAverage()
      : _sum(0)
      , _index(0)
      , _count(0) { }

    void addNumber(T num) {
        if (_count < WINDOW_SIZE) {
            _count++;
        } else {
            _sum -= _window[_index];
        }

        _window[_index] = num;
        _sum += num;
        _index = (_index + 1) % WINDOW_SIZE;
    }

    float getAverage() const {
        if (_count == 0) { return 0.0; }
        return static_cast<float>(_sum) / _count;
    }

private:
    std::array<T, WINDOW_SIZE> _window;
    T _sum;
    size_t _index;
    size_t _count;
};

class VeDirectMpptController : public VeDirectFrameHandler<veMpptStruct> {
public:
    VeDirectMpptController() = default;

    void init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging, uint8_t hwSerialPort);

    using data_t = veMpptStruct;

    void loop() final;

private:
    bool hexDataHandler(VeDirectHexData const &data) final;
    bool processTextDataDerived(std::string const& name, std::string const& value) final;
    void frameValidEvent() final;
    MovingAverage<float, 5> _efficiency;
    int8_t _slotNr = 0;
#ifdef PROCESS_NETWORK_STATE
    std::array<VeDirectHexRegister, 17> _slotRegister {
#else
    std::array<VeDirectHexRegister, 14> _slotRegister {
#endif
                                                        VeDirectHexRegister::Capabilities,
                                                        VeDirectHexRegister::BatteryType,
                                                        VeDirectHexRegister::ChargeControllerTemperature,
                                                        VeDirectHexRegister::NetworkTotalDcInputPower,
//                                                        VeDirectHexRegister::ChargerVoltage,
//                                                        VeDirectHexRegister::ChargerCurrent,
                                                        VeDirectHexRegister::ChargerMaximumCurrent,
//                                                        VeDirectHexRegister::LoadOutputVoltage,
                                                        VeDirectHexRegister::LoadOutputState,
//                                                        VeDirectHexRegister::LoadOutputControl,
                                                        VeDirectHexRegister::LoadCurrent,
                                                        VeDirectHexRegister::PanelCurrent,
                                                        VeDirectHexRegister::BatteryMaximumCurrent,
                                                        VeDirectHexRegister::VoltageSettingsRange,
                                                        VeDirectHexRegister::BatteryVoltageSetting,
                                                        VeDirectHexRegister::SmartBatterySenseTemperature,
                                                        VeDirectHexRegister::BatteryFloatVoltage,
                                                        VeDirectHexRegister::BatteryAbsorptionVoltage
#ifdef PROCESS_NETWORK_STATE
                                                        ,
    	                                                VeDirectHexRegister::NetworkInfo,
	                                                    VeDirectHexRegister::NetworkMode,
	                                                    VeDirectHexRegister::NetworkStatus,
#endif
                                                         };
};
