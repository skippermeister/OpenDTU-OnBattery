#ifdef USE_ModbusDTU

#include "ModbusDTU.h"
#include "Datastore.h"
#include "MessageOutput.h"

ModbusIP mb;

ModbusDtuClass ModbusDtu;

ModbusDtuClass::ModbusDtuClass()
    : _loopTask(Configuration.get().Dtu.PollInterval * TASK_SECOND, TASK_FOREVER, std::bind(&ModbusDtuClass::loop, this))
    , _mbloopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ModbusDtuClass::mbloop, this))
{
}

void ModbusDtuClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize ModbusDTU... ");

    scheduler.addTask(_loopTask);
    _loopTask.enable();
    scheduler.addTask(_modbusTask);

    MessageOutput.println("done");
}


void ModbusDtuClass::modbus()
{
    mb.task();
}

void ModbusDtuClass::setup()
{
    MessageOutput.print("Starting ModbusDTU Server... ");

    auto const& config = Configuration.get();

    if ((config.Dtu.Serial) < 0x100000000000 || (config.Dtu.Serial) > 0x199999999999) {
#ifdef SIMULATE_HOYMILES_DTU
        MessageOutput.printf("Hoymiles DTU Simulation: need a DTU Serial between 100000000000 and 199999999999 (currently configured: %llx)\r\n", config.Dtu.Serial);
#else
        MessageOutput.printf("Modbus: need a DTU Serial between 100000000000 and 199999999999 (currently configured: %llx)\r\n", config.Dtu.Serial);
#endif
        _isstarted = false;
        return;
    }

    mb.server();
#ifdef SIMULATE_HOYMILES_DTU
    mb.addHreg(0x2000, (config.Dtu.Serial >> 32) & 0xFFFF);
    mb.addHreg(0x2001, (config.Dtu.Serial >> 16) & 0xFFFF);
    mb.addHreg(0x2002, (config.Dtu.Serial) & 0xFFFF);

    uint8_t channels = 0;
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }
        uint64_t serialInv = inv->serial();
        for (auto& t : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(t)) {
                if (t == TYPE_DC) {
                    mb.addHreg(channels * 3 + 0x2056, (serialInv >> 16) & 0xFFFF);
                    mb.addHreg(channels * 3 + 0x2057, (serialInv >> 8) & 0xFFFF);
                    mb.addHreg(channels * 3 + 0x2058, serialInv & 0xFFFF);
                    channels++;
                }
            }
        }
    }

    for (uint8_t i = 0; i <= channels; i++) {
        for (uint8_t j = 0; j < 20; j++) {
            mb.addHreg(i * 20 + 0x1000 + j, 0);
        }
        yield();
    }

    mb.addHreg(0x2501, 502); // Ethernet Port Number

#else
    // simulate Fornius
    mb.addHreg(40000, 0x5375); // 40001 0x5375 Su
    mb.addHreg(40001, 0x6E53); // 0x6E53 nS
    mb.addHreg(40002, 1); // indetifies this as a SunSpec common model block
    mb.addHreg(40003, 65); // length of common model block
    addHString(40004, config.modbus.mfrname, 16); // Manufacturer start
    addHString(40020, config.modbus.modelname, 16); // Device Model start
    addHString(40036, config.modbus.options, 8); // Options start
    addHString(40044, config.modbus.version, 8); // Software Version start
    const char *serialconfig = config.modbus.serial;
    if (!strlen(serialconfig)) {
        char serial[24];
        snprintf(serial,sizeof(serial),"%llx",(config.Dtu.Serial));
        MessageOutput.printf("Modbus: init uses DTU Serial: %s\r\n", serial);
        addHString(40052, buff, 6); // Serial Number start
        mb.addHreg(40058, 0, 10);
    } else {
        for (uint8_t i = 0; i < 32; i += 2) {
            uint16_t value = 0;
            if (strlen(serialconfig) > i) value = (serialconfig[i] << 8) | (i + 1 < strlen(serialconfig) ? serialconfig[i + 1] : 0);
            mb.addHreg(40052 + (i / 2), value); //40052 - 40067 Serial Number
            // MessageOutput.printf("Modbus: write %d to register %d\r\n", value, (40052 + (i / 2)));
        }
    }

    mb.addHreg(40068, 202); // Modbus TCP Address: 202
    mb.addHreg(40069, 213);   //40069 SunSpec_DID
    mb.addHreg(40070, 124);   //40070 SunSpec_Length
    mb.addHreg(40071, 0, 123);//40071 - 40194 smartmeter data
    mb.addHreg(40195, 65535); //40195 end block identifier
    mb.addHreg(40196, 0);     //40196
#endif

    _isstarted = true;

    MessageOutput.println("done");
}

void ModbusDtuClass::loop()
{
    auto const& config = Configuration.get();

    _loopTask.setInterval(config.Dtu.PollInterval * TASK_SECOND);

    if (!(config.Modbus.modbus_tcp_enabled)) return;

    if (!Hoymiles.isAllRadioIdle()) {
         _loopTask.forceNextIteration();
         return;
    }

    if (!_isstarted) {
        if (!config.modbus.modbus_delaystart ||
            (Datastore.getIsAllEnabledReachable() && Datastore.getTotalAcYieldTotalEnabled() != 0))
        {
            MessageOutput.printf("Modbus: starting server ...\r\n");
            ModbusDtu.setup();
            _modbusTask.enable();
        } else {
            MessageOutput.printf("Modbus: not initializing yet! (Total Yield = 0 or not all configured inverters reachable)\r\n");
            return;
        }
    }

#ifdef SIMULATE_HOYMILES_DTU
    uint8_t chan = 0;
    uint8_t invNumb = 0;

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }
        // Loop all channels
        for (auto& t : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(t)) {
                if (t == TYPE_DC) {
                    uint64_t serialInv = inv->serial();
                    mb.Hreg(chan * 20 + 0x1000, 0x3C00 + ((serialInv >> 40) & 0xFF));
                    mb.Hreg(chan * 20 + 0x1001, (serialInv >> 24) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x1002, (serialInv >> 8) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x1003, (serialInv << 8) + c + 1);
                    if (inv->Statistics()->getStringMaxPower(c) > 0 && inv->Statistics()->getChannelFieldValue(t, c, FLD_IRR) < 500) {
                        mb.Hreg(chan * 20 + 0x1004, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_UDC)) * 10);
                        mb.Hreg(chan * 20 + 0x1005, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_IDC)) * 100);
                        mb.Hreg(chan * 20 + 0x1008, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_PDC)) * 10);
                    } else if (inv->Statistics()->getStringMaxPower(c) == 0) {
                        mb.Hreg(chan * 20 + 0x1004, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_UDC)) * 10);
                        mb.Hreg(chan * 20 + 0x1005, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_IDC)) * 100);
                        mb.Hreg(chan * 20 + 0x1008, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_PDC)) * 10);
                    }

                    mb.Hreg(chan * 20 + 0x1006, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC)) * 10);
                    mb.Hreg(chan * 20 + 0x1007, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_F)) * 100);
                    mb.Hreg(chan * 20 + 0x1009, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_YD)));
                    mb.Hreg(chan * 20 + 0x100A, static_cast<uint16_t>(static_cast<uint32_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_YT)) * 1000 >> 16) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x100B, static_cast<uint16_t>(static_cast<uint32_t>(inv->Statistics()->getChannelFieldValue(t, c, FLD_YT)) * 1000) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x100C, static_cast<uint16_t>(inv->Statistics()->getChannelFieldValue(TYPE_INV, CH0, FLD_T)) * 10);
                    mb.Hreg(chan * 20 + 0x100D, 3);
                    mb.Hreg(chan * 20 + 0x100E, 0);
                    mb.Hreg(chan * 20 + 0x100F, 0);
                    mb.Hreg(chan * 20 + 0x1010, 0x0107);
                    mb.Hreg(chan * 20 + 0x1011, 0);
                    mb.Hreg(chan * 20 + 0x1012, 0);
                    mb.Hreg(chan * 20 + 0x1013, 0);

                    chan++;
                }
            }
        }
        invNumb++;
    }
#else
    if (!Datastore.getIsAllEnabledReachable() ||
        !(Datastore.getTotalAcYieldTotalEnabled() != 0) ||
        !_isstarted ||
        !config.modbus.modbus_delaystart)
    {
        MessageOutput.printf("Modbus: not updating registers! (Total Yield = 0 or not all configured inverters reachable)\r\n");
        return;
    } else {
        setHRegs(40097, Datastore.getTotalAcPowerEnabled() * -1);
        float value = (Datastore.getTotalAcYieldTotalEnabled() * 1000);
        if (value > _lasttotal) {
            _lasttotal = value;
            setHRegs(40129, value);
        }

        if (Hoymiles.getNumInverters() == 1) {
            auto inv = Hoymiles.getInverterByPos(0);
            if (inv != nullptr) {
                for (auto& t : inv->Statistics()->getChannelTypes()) {
                    if (t == TYPE_DC) {
                        setHregs(40071, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC));
                        setHregs(40073, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1) != 0 ? inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1) : inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC));
                        setHRegs(40075, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_2));
                        setHRegs(40077, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_3));
                        setHRegs(40079, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N));
                        setHRegs(40081, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N) != 0 ? inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N) : inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC));
                        setHRegs(40083, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N));
                        setHRegs(40085, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_3N));
                        setHRegs(40087, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC));
                        setHRegs(40089, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_12));
                        setHRegs(40091, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_23));
                        setHRegs(40093, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_31));
                        setHRegs(40095, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_F));
                        //setHRegs(40097, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC)*-1); //done above already!
                        setHRegs(40099, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1) != 0 ?
                                        inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N) * -1 :
                                        inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC) * -1;
                        setHRegs(40101, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_2) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N) * -1);
                        setHRegs(40103, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_3) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_3N) * -1);

                        setHRegs(40113, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_Q));  // apparent power

                        setHRegs(40121, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PF)); // Power Factor

                        //value = inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_YT)*1000.0;  // AC Lifetime Energy Production
                        //if (value > _lastttotal) {
                        //    _lasttotal = value;
                        //    setHRegs(40129, value);
                        //}
                    }
                }
            }
            // MessageOutput.printf("Modbus: End additional SM Information\r\n");
        }
    }
#endif
}

void ModbusDtuClass::mbloop()
{
    if (!(Configuration.get().Modbus.Fronius_SM_Simulation_Enabled) || !_isstarted) {
        return;
    }

    mb.task();
}

void ModbusDtuClass::setHRegs(uint16_t reg, float value)
{
    uint16_t* hexbytes = reinterpret_cast<uint16_t*>(&value);
    mb.Hreg(reg++, hexbytes[1]);
    mb.Hreg(reg, hexbytes[0]);
}

void ModbusDtuClass::addHString(uint16_t reg, const char* s, uint8_t len)
{
    len *= 2; // convert to number of bytes
    for (uint8_t i = 0; i < len; i += 2) {
        uint16_t value = 0;
        if (i < strlen(s))
            value = *(reinterpret_cast<uint16_t*>(&s[i]));
        mb.addHreg(reg++, value);
    }
}

#endif
