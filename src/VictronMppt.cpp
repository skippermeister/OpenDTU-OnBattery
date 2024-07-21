// SPDX-License-Identifier: GPL-2.0-or-later
#include "VictronMppt.h"
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "SerialPortManager.h"

VictronMpptClass VictronMppt;

static constexpr char TAG[] = "[VictronMppt]";

void VictronMpptClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize VE.Direct interface... ");

    scheduler.addTask(_loopTask);
    _loopTask.setCallback([this] { loop(); });
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();

    MessageOutput.println("done");
}

void VictronMpptClass::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();
    for (auto const& o: _serialPortOwners) {
        SerialPortManager.freePort(o.c_str());
    }
    _serialPortOwners.clear();

    if (!Configuration.get().Vedirect.Enabled) {
        MessageOutput.print("not enabled... ");
        return;
    }

    const PinMapping_t& pin = PinMapping.get();

    initController(pin.victron_rx, pin.victron_tx, _verboseLogging, 1);

    initController(pin.victron_rx2, pin.victron_tx2, _verboseLogging, 2);

    initController(pin.victron_rx3, pin.victron_tx3, _verboseLogging, 3);
}

bool VictronMpptClass::initController(int8_t rx, int8_t tx, bool logging, uint8_t instance)
{
    MessageOutput.printf("%s%s RS232 port rx = %d, tx = %d, instance = %d ", TAG, __FUNCTION__, rx, tx, instance);

    if (rx < 0 && tx < 0) {
        MessageOutput.printf("not configued\r\n");
        return false;
    }
    if (rx == tx || rx < 0 || tx < 0) {
        MessageOutput.printf("invalid pin config\r\n");
        return false;
    }
    MessageOutput.println();

    String owner("Victron MPPT ");
    owner += String(instance);
    auto oHwSerialPort = SerialPortManager.allocatePort(owner.c_str());
    if (!oHwSerialPort) { return false; }

    _serialPortOwners.push_back(owner);

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, &MessageOutput, logging, *oHwSerialPort);
    _controllers.push_back(std::move(upController));
    return true;
}

void VictronMpptClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        upController->loop();
    }
}

/*
 * isDataValid()
 * return: true = if at least one of the MPPT controllers delivers valid data
 */
bool VictronMpptClass::isDataValid() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        if (upController->isDataValid()) { return true; }
    }

    return !_controllers.empty();
}

bool VictronMpptClass::isDataValid(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) {
        return false;
    }

    return _controllers[idx]->isDataValid();
}

uint32_t VictronMpptClass::getDataAgeMillis() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty()) { return 0; }

    auto now = millis();

    auto iter = _controllers.cbegin();
    uint32_t age = now - (*iter)->getLastUpdate();
    ++iter;

    while (iter != _controllers.end()) {
        age = std::min<uint32_t>(age, now - (*iter)->getLastUpdate());
        ++iter;
    }

    return age;
}

uint32_t VictronMpptClass::getDataAgeMillis(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) { return 0; }

    return millis() - _controllers[idx]->getLastUpdate();
}

std::optional<VeDirectMpptController::data_t> VictronMpptClass::getData(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) {
        MessageOutput.printf("%s ERROR: MPPT controller index %d is out of bounds (%d controllers)\r\n", TAG,
            idx, _controllers.size());
        return std::nullopt;
    }

    if (!_controllers[idx]->isDataValid()) { return std::nullopt; }

    return _controllers[idx]->getData();
}

int32_t VictronMpptClass::getPowerOutputWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }

        // if any charge controller is part of a VE.Smart network, and if the
        // charge controller is connected in a way that allows to send
        // requests, we should have the "network total DC input power"
        // available. if so, to estimate the output power, we multiply by
        // the calculated efficiency of the connected charge controller.
        auto networkPower = upController->getData().NetworkTotalDcInputPowerMilliWatts;
        if (networkPower.first > 0) {
            return static_cast<int32_t>(networkPower.second / 1000.0 * upController->getData().mpptEfficiency_Percent / 100);
        }

        sum += upController->getData().batteryOutputPower_W;
    }

    return sum;
}

int32_t VictronMpptClass::getPanelPowerWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }

        // if any charge controller is part of a VE.Smart network, and if the
        // charge controller is connected in a way that allows to send
        // requests, we should have the "network total DC input power" available.
        auto networkPower = upController->getData().NetworkTotalDcInputPowerMilliWatts;
        if (networkPower.first > 0) {
            return static_cast<int32_t>(networkPower.second / 1000.0);
        }

        sum += upController->getData().panelPower_PPV_W;
    }

    return sum;
}

float VictronMpptClass::getYieldTotal() const
{
    float sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData().yieldTotal_H19_Wh/1000.0;
    }

    return sum;
}

float VictronMpptClass::getYieldDay() const
{
    float sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData().yieldToday_H20_Wh/1000.0;
    }

    return sum;
}

float VictronMpptClass::getOutputVoltage() const
{
    float min = -1;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        float volts = upController->getData().batteryVoltage_V_mV/1000.0;
        if (min == -1) { min = volts; }
        min = std::min(min, volts);
    }

    return min;
}
