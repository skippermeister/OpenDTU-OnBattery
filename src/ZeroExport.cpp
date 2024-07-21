
#include "ZeroExport.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PowerMeter.h"
#include <frozen/map.h>

ZeroExportClass ZeroExport;

#define ZERO_EXPORT_DEBUG

static constexpr char TAG[] = "[ZeroExport]";

frozen::string const& ZeroExportClass::getStatusText(ZeroExportClass::Status status)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Status, frozen::string, 15> texts = {
        { Status::Initializing, "initializing (should not see me)" },
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::WaitingForValidTimestamp, "waiting for valid date and time to be available" },
        { Status::PowerMeterDisabled, "no power meter is configured/enabled" },
        { Status::PowerMeterTimeout, "power meter readings are outdated" },
        { Status::PowerMeterPending, "waiting for sufficiently recent power meter reading" },
        { Status::InverterInvalid, "invalid inverter selection/configuration" },
        { Status::InverterOffline, "inverter is offline (polling enabled? radio okay?)" },
        { Status::InverterCommandsDisabled, "inverter configuration prohibits sending commands" },
        { Status::InverterLimitPending, "waiting for a power limit command to complete" },
        { Status::InverterPowerCmdPending, "waiting for a start/stop/restart command to complete" },
        { Status::InverterDevInfoPending, "waiting for inverter device information to be available" },
        { Status::InverterStatsPending, "waiting for sufficiently recent inverter data" },
        { Status::Settling, "waiting for the system to settle" },
        { Status::Stable, "the system is stable, the last power limit is still valid" },
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) {
        return missing;
    }

    return iter->second;
}

void ZeroExportClass::announceStatus(ZeroExportClass::Status status, bool forceLogging)
{
    // this method is called with high frequency. print the status text if
    // the status changed since we last printed the text of another one.
    // otherwise repeat the info with a fixed interval.
    if (_lastStatus == status && !_lastStatusPrinted.occured()) {
        return;
    }

    // after announcing once that the DPL is disabled by configuration, it
    // should just be silent while it is disabled.
    if (status == Status::DisabledByConfig && _lastStatus == status) {
        return;
    }

    if (forceLogging || _verboseLogging)
        MessageOutput.printf("%s %s\r\n", TAG, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

ZeroExportClass::ZeroExportClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ZeroExportClass::loop, this))
{
}

void ZeroExportClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Dynamic Zero Grid Export Limiter... ");

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _totalMaxPower = 0;
    _invID = 0;

    _last_timeStamp = millis();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        _calculationBackoffMs[i].set(0);
    }

    _lastStatusPrinted.set(10 * 1000);

    MessageOutput.println("done");
}

void ZeroExportClass::loop()
{

    const ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) {
        announceStatus(Status::WaitingForValidTimestamp, true);
        return;
    }

    // find sequentially a valid inverter serial no
    while (1) {
        if (_invID >= INV_MAX_COUNT) _invID = 0;    // switch back to first inverter serial no in list
        if (cZeroExport.serials[_invID] == 0) {
            if (_invID == 0) {
                // list is empty, no inverters are selected
                announceStatus(Status::InverterInvalid, true);
                return;
            }
            _invID = 0; // we are at end of entries in the list, switch back to first inverter in list
        } else {
            // found a valid inverter serial no
            break;
        }
    }
    _inverter = Hoymiles.getInverterBySerial(cZeroExport.serials[_invID]);

    if (_inverter == nullptr) {
        announceStatus(Status::InverterInvalid, true);
        _invID++; // switch to next selected inverter
        return;
    }

    if (!_inverter->getEnablePolling()) {
        _invID++;
        return;
    }

    // data polling is disabled or the inverter is deemed offline
    if (!_inverter->isReachable()) {
        announceStatus(Status::InverterOffline);
        _invID++;
        return;
    }

    // sending commands to the inverter is disabled
    if (!_inverter->getEnableCommands()) {
        announceStatus(Status::InverterCommandsDisabled);
        _invID++;
        return;
    }

    // concerns active power commands (power limits) only (also from web app or MQTT)
    auto lastLimitCommandState = _inverter->SystemConfigPara()->getLastLimitCommandSuccess();
    if (CMD_PENDING == lastLimitCommandState) {
        announceStatus(Status::InverterLimitPending);
        _invID++;
        return;
    }

    // concerns power commands (start, stop, restart) only (also from web app or MQTT)
    auto lastPowerCommandState = _inverter->PowerCommand()->getLastPowerCommandSuccess();
    if (CMD_PENDING == lastPowerCommandState) {
        announceStatus(Status::InverterPowerCmdPending);
        _invID++;
        return;
    }

    // a calculated power limit will always be limited to the reported
    // device's max power. that upper limit is only known after the first
    // DevInfoSimpleCommand succeeded.
    if (_inverter->DevInfo()->getMaxPower() <= 0) {
        announceStatus(Status::InverterDevInfoPending);
        _invID++;
        return;
    }

    // calculate the maximum possible power of all selected inverters
    // each bid in the _invIDmask represents one inverter
    // if a bid is set, the related inverter max power value has been added to _totalMaxPower
    static uint16_t _invIDmask = 0;
    if (!(_invIDmask & (1 << _invID))) {
        _totalMaxPower += _inverter->DevInfo()->getMaxPower();
        _invIDmask |= (1 << _invID);
        MessageOutput.printf("%s Inverter ID %" PRIx64 " MaxPower = %d, TotalMaxPower = %d Watt\r\n", TAG,
            _inverter->serial(), _inverter->DevInfo()->getMaxPower(), _totalMaxPower);
    }

    if (!cZeroExport.Enabled) {
        announceStatus(Status::DisabledByConfig);
        setNewPowerLimit(_inverter, 100); // set 100%
        _invID++;
        return;
    }

    if (!Configuration.get().PowerMeter.Enabled) {
        announceStatus(Status::PowerMeterDisabled, true);
        return;
    }

    if (millis() - PowerMeter.getLastUpdate() > (30 * 1000)) {
        announceStatus(Status::PowerMeterTimeout, true);
        return;
    }

    // concerns both power limits and start/stop/restart commands and is
    // only updated if a respective response was received from the inverter
    auto lastUpdateCmd = std::max(
        _inverter->SystemConfigPara()->getLastUpdateCommand(),
        _inverter->PowerCommand()->getLastUpdateCommand());

    // wait for power meter and inverter stat updates after a settling phase
    auto settlingEnd = lastUpdateCmd + 3 * 1000;

    if (millis() < settlingEnd) {
        announceStatus(Status::Settling);
        _invID++;
        return;
    }

    if (_inverter->Statistics()->getLastUpdate() <= settlingEnd) {
        announceStatus(Status::InverterStatsPending);
        _invID++;
        return;
    }

    if (PowerMeter.getLastUpdate() <= settlingEnd) {
        announceStatus(Status::PowerMeterPending);
        _invID++;
        return;
    }

    // since _lastCalculation and _calculationBackoffMs are initialized to
    // zero, this test is passed the first time the condition is checked.
    if (!_calculationBackoffMs[_invID].occured()) {
        announceStatus(Status::Stable);
        _invID++;
        return;
    }

    // if (_verboseLogging) MessageOutput.printf("%s ******************* ENTER **********************\r\n", TAG);

    int16_t newPowerLimit = pid_Regler();

    bool limitUpdated = setNewPowerLimit(_inverter, newPowerLimit);

    // if (_verboseLogging) MessageOutput.printf("%s ******************* Leaving *******************\r\n", TAG);

    if (!limitUpdated) {
        // increase polling backoff if system seems to be stable
        _calculationBackoffMs[_invID].set(std::min<uint32_t>(1024, _calculationBackoffMs[_invID].get() * 2));
        announceStatus(Status::Stable);
        _invID++;
        return;
    }

    _calculationBackoffMs[_invID].set(_calculationBackoffMsDefault);

    _invID++;
}

bool ZeroExportClass::setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int16_t newPowerLimit)
{

    ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;

    // Check if the new value is within the limits of the hysteresis
    auto diff = std::abs(newPowerLimit - _lastRequestedPowerLimit);
    if (diff < cZeroExport.PowerHysteresis) {

        if (_verboseLogging)
            MessageOutput.printf("%s reusing old limit: %d %%, diff: %d %%, hysteresis: %d %%\r\n", TAG,
                _lastRequestedPowerLimit, diff, cZeroExport.PowerHysteresis);

        return false;
    }

    commitPowerLimit(inverter, newPowerLimit, true);

    return true;
}

void ZeroExportClass::commitPowerLimit(std::shared_ptr<InverterAbstract> inverter, int16_t limit, bool enablePowerProduction)
{
    // disable power production as soon as possible.
    // setting the power limit is less important.
    if (!enablePowerProduction && inverter->isProducing()) {
        if (_verboseLogging)
            MessageOutput.printf("%s Stopping inverter...", TAG);
        inverter->sendPowerControlRequest(false);
    }

    inverter->sendActivePowerControlRequest(static_cast<float>(limit), PowerLimitControlType::RelativNonPersistent);

    _lastRequestedPowerLimit = limit;

    // enable power production only after setting the desired limit,
    // such that an older, greater limit will not cause power spikes.
    if (enablePowerProduction && !inverter->isProducing()) {
        if (_verboseLogging)
            MessageOutput.printf("%s Starting up inverter...", TAG);
        inverter->sendPowerControlRequest(true);
    }
}

int16_t ZeroExportClass::pid_Regler(void)
{
    _timeStamp = millis();

    int16_t payload;

    const ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;

    // static_cast<unsigned int>(_inverter->Statistics()->getChannelFieldDigits(TYPE_AC, CH0, FLD_PAC)));

    float p = (100.0 / _totalMaxPower) * (PowerMeter.getPowerTotal() + cZeroExport.MaxGrid);
    _last_I = _actual_I;
    _actual_I = p * (_timeStamp - _last_timeStamp) / (1000 * cZeroExport.Tn);
    _last_timeStamp = _timeStamp;
    payload = (int16_t)round(_last_payload + p + _actual_I);

    // between 10 and 100%
    if (payload > 100) {
        payload = 100;
        _actual_I = _last_I;
    } else if (payload < cZeroExport.MinimumLimit) { // not below 10%, because this will switch of the inverter
        payload = cZeroExport.MinimumLimit;
        _actual_I = _last_I;
    }

    _last_payload = payload;

    return payload;
}

void ZeroExportClass::setParameter(float value, ZeroExportTopic parameter)
{
    switch (parameter) {
    case ZeroExportTopic::Enabled:
        Configuration.get().ZeroExport.Enabled = static_cast<int>(value) == 0 ? false : true;
        break;
    case ZeroExportTopic::MaxGrid:
        Configuration.get().ZeroExport.MaxGrid = value;
        break;
    case ZeroExportTopic::MinimumLimit:
        Configuration.get().ZeroExport.MinimumLimit = value;
        break;
    case ZeroExportTopic::PowerHysteresis:
        Configuration.get().ZeroExport.PowerHysteresis = value;
        break;
    case ZeroExportTopic::Tn:
        Configuration.get().ZeroExport.Tn = value;
        break;
    }
}
