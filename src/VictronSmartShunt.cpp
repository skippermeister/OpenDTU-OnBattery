// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_VICTRON_SMART_SHUNT

#include "VictronSmartShunt.h"
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "SerialPortManager.h"

static constexpr char TAG[] = "[VictronSmartShunt]";

void VictronSmartShunt::deinit()
{
    SerialPortManager.freePort(_serialPortOwner);

    _initialized = false;

    MessageOutput.printf("%s Serial driver uninstalled\r\n", TAG);
}

bool VictronSmartShunt::init()
{
    MessageOutput.printf("Initialize Ve.Direct interface...");

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf(" rx = %d, tx = %d", pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0) {
        MessageOutput.println(" Invalid pin config");
        return false;
    }
    MessageOutput.println();

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    VeDirectShunt.init(rx, tx, &MessageOutput, _verboseLogging, *oHwSerialPort);

    _initialized = true;

    return true;
}

void VictronSmartShunt::loop()
{
    VeDirectShunt.loop();

    if (VeDirectShunt.getLastUpdate() <= _lastUpdate) { return; }

    _stats->updateFrom(VeDirectShunt.getData());
    _lastUpdate = VeDirectShunt.getLastUpdate();
}

#endif
