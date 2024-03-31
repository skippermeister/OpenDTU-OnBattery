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
    SerialPortManager.invalidateMpptPorts();

    if (!Configuration.get().Vedirect.Enabled) {
        MessageOutput.print("not enabled... ");
        return;
    }

    const PinMapping_t& pin = PinMapping.get();

    int hwSerialPort = 1;

    bool initSuccess = initController(pin.victron_rx, pin.victron_tx, _verboseLogging, hwSerialPort);
    if (initSuccess) {
        hwSerialPort++;
    }

    // only initialize if rx2 and tx2 pin is configured
    if (pin.victron_rx2 > 0 && pin.victron_tx2 >= 0) initController(pin.victron_rx2, pin.victron_tx2, _verboseLogging, hwSerialPort);
}

bool VictronMpptClass::initController(int8_t rx, int8_t tx, bool logging, int hwSerialPort)
{
    MessageOutput.printf("%s%s RS232 port rx = %d, tx = %d, hwSerialPort = %d\r\n", TAG, __FUNCTION__, rx, tx, hwSerialPort);

    if (rx < 0) {
        MessageOutput.printf("%s%s invalid pin config\r\n", TAG, __FUNCTION__);
        return false;
    }

    if (!SerialPortManager.allocateMpptPort(hwSerialPort)) {
        MessageOutput.printf("%s%s Serial port %d already in use. Initialization aborted!\r\n", TAG, __FUNCTION__,
                             hwSerialPort);
        return false;
    }

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, &MessageOutput, logging, hwSerialPort);
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

bool VictronMpptClass::isDataValid() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        if (!upController->isDataValid()) { return false; }
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

std::optional<VeDirectMpptController::spData_t> VictronMpptClass::getData(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) {
        MessageOutput.printf("%s ERROR: MPPT controller index %d is out of bounds (%d controllers)\r\n", TAG,
            idx, _controllers.size());
        return std::make_shared<VeDirectMpptController::veMpptStruct>();
    }

    return _controllers[idx]->getData();
}

int32_t VictronMpptClass::getPowerOutputWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData()->P;
    }

    return sum;
}

int32_t VictronMpptClass::getPanelPowerWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData()->PPV;
    }

    return sum;
}

double VictronMpptClass::getYieldTotal() const
{
    double sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData()->H19;
    }

    return sum;
}

double VictronMpptClass::getYieldDay() const
{
    double sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData()->H20;
    }

    return sum;
}

double VictronMpptClass::getOutputVoltage() const
{
    double min = -1;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        double volts = upController->getData()->V;
        if (min == -1) { min = volts; }
        min = std::min(min, volts);
    }

    return min;
}
