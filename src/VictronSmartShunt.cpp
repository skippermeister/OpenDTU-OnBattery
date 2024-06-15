// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_VICTRON_SMART_SHUNT

#include "VictronSmartShunt.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"

void VictronSmartShunt::deinit()
{
    SerialPortManager.freePort(_serialPortOwner);

    MessageOutput.printf("%s Serial driver uninstalled\r\n", TAG);
}

bool VictronSmartShunt::init()
{
    MessageOutput.println("Initialize Ve.Direct interface...");

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf(" rx = %d, tx = %d\r\n", pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0) {
        MessageOutput.println(" Invalid pin config");
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    VeDirectShunt.init(rx, tx, &MessageOutput, VictronMppt.getVerboseLogging(), *oHwSerialPort);
    return true;
}

void VictronSmartShunt::loop()
{
    VeDirectShunt.loop();

    if (VeDirectShunt.getLastUpdate() <= _lastUpdate) {
        return;
    }

    _stats->updateFrom(VeDirectShunt.veFrame);
    _lastUpdate = VeDirectShunt.getLastUpdate();
}

#endif
