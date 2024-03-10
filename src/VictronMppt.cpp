// SPDX-License-Identifier: GPL-2.0-or-later
#include "VictronMppt.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"

VictronMpptClass VictronMppt;

static constexpr char TAG[] = "[VictronMppt]";

void VictronMpptClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize VE.Direct interface... ");

    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&VictronMpptClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();

    MessageOutput.println("done");
}

void VictronMpptClass::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();

    if (!Configuration.get().Vedirect.Enabled) {
        return;
    }

    const PinMapping_t& pin = PinMapping.get();
    int8_t rx = pin.victron_rx;
    int8_t tx = pin.victron_tx;

    MessageOutput.printf("RS232 port rx = %d, tx = %d. ", rx, tx);

    if (rx < 0) {
        MessageOutput.println("Invalid pin config !");
        return;
    }

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, &MessageOutput, _verboseLogging);
    _controllers.push_back(std::move(upController));
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
        if (!upController->isDataValid()) {
            return false;
        }
    }

    return !_controllers.empty();
}

uint32_t VictronMpptClass::getDataAgeMillis() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty()) {
        return 0;
    }

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

VeDirectMpptController::spData_t VictronMpptClass::getData(size_t idx) const
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
        sum += upController->getData()->P;
    }

    return sum;
}

int32_t VictronMpptClass::getPanelPowerWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        sum += upController->getData()->PPV;
    }

    return sum;
}

double VictronMpptClass::getYieldTotal() const
{
    double sum = 0;

    for (const auto& upController : _controllers) {
        sum += upController->getData()->H19;
    }

    return sum;
}

double VictronMpptClass::getYieldDay() const
{
    double sum = 0;

    for (const auto& upController : _controllers) {
        sum += upController->getData()->H20;
    }

    return sum;
}

double VictronMpptClass::getOutputVoltage() const
{
    double min = -1;

    for (const auto& upController : _controllers) {
        double volts = upController->getData()->V;
        if (min == -1) {
            min = volts;
        }
        min = std::min(min, volts);
    }

    return min;
}
