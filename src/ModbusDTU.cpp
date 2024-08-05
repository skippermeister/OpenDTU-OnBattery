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
    _modbusTask.enable();

    MessageOutput.println("done");
}

void ModbusDtuClass::modbus()
{
    if (_isstarted) mb.task();
}

void ModbusDtuClass::setup()
{
    MessageOutput.print("Starting ModbusDTU Server... ");

    if ((Configuration.get().Dtu.Serial) < 0x100000000000 || (Configuration.get().Dtu.Serial) > 0x199999999999) {
#ifdef SIMULATE_HOYMILES_DTU
        MessageOutput.printf("Hoymiles DTU Simulation: need a DTU Serial between 100000000000 and 199999999999 (currently configured: %llx)\r\n", Configuration.get().Dtu.Serial);
#else
        MessageOutput.printf("Fronius SM Simulation: need a DTU Serial between 100000000000 and 199999999999 (currently configured: %llx)\r\n", Configuration.get().Dtu.Serial);
#endif
        _isstarted = false;
        return;
    }

    mb.server();
#ifdef SIMULATE_HOYMILES_DTU
    const CONFIG_T& config = Configuration.get();
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
    addHString(40004, "Fronius", 16); // Manufacturer start
    addHString(40020, "Smart Meter TS 65A-3", 16); // Device Model start
    addHString(40036, "<primary>", 8); // Options start
    addHString(40044, "1.3", 8); // Software Version start
    char buff[24];
    snprintf(buff,sizeof(buff),"%llx",(Configuration.get().Dtu.Serial));
    MessageOutput.printf("Fronius SM Simulation: init uses DTU Serial: %llx\r\n", Configuration.get().Dtu.Serial);
    addHString(40052, buff, 16); // Serial Number start
    mb.addHreg(40068, 202); // Modbus TCP Address: 202

    uint8_t offset = 40069;
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }
        uint64_t serialInv = inv->serial();

        mb.addHreg(offset, 111);
        mb.addHreg(offset + 1, 60);
        mb.addHreg(offset + 2, 0, 58);
        offset += 60;

        uint8_t MPPT_offset = offset;
        mb.addHreg(offset, 160);
        mb.addHreg(offset + 1, 0, 7);
        offset += 8;

        uint8_t channel = 0;
        for (auto& t : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(t)) {
                if (t == TYPE_DC) {
                    mb.addHreg(offset, ++channel);
                    char buf[16];
                    memset(buf, 0, sizeof(buf);
                    snprintf(buf, sizeof(buf), "String %d", channel);
                    addHString(offset + 1, buf, 8);
                    mb.addHreg(offset + 9, 0, 11);
                    offset += 20;
                }
            }
        }
        mb.Hreg(MPPToffset + 1, channel * 20 + 8); // Length of Multiple MPPT Inverter extension model
        mb.Hreg(MPPToffset + 7, channel); // number of channels
    }

    mb.addHreg(offset++, 0xFFFF); // 40432 end block identifier
    mb.addHreg(offset, 0); // 40433

#endif

    _isstarted = true;

    MessageOutput.println("done");
}

void ModbusDtuClass::loop()
{
    _loopTask.setInterval(Configuration.get().Dtu.PollInterval * TASK_SECOND);

    if (!(Configuration.get().Modbus.Fronius_SM_Simulation_Enabled)) return;

    if (!Hoymiles.isAllRadioIdle()) {
         _loopTask.forceNextIteration();
         return;
    }

    if (!_isstarted) {
        if (Datastore.getIsAllEnabledReachable() && Datastore.getTotalAcYieldTotalEnabled() != 0) {
            ModbusDtu.setup();
        } else {
            MessageOutput.printf("Fronius SM Simulation: not initializing yet! (Total Yield = 0 or not all configured inverters reachable).\r\n");
            return;
        }
    }

    if (!Hoymiles.isAllRadioIdle()) {
        return;
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
                        mb.Hreg(chan * 20 + 0x1004, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_UDC) * 10));
                        mb.Hreg(chan * 20 + 0x1005, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_IDC) * 100));
                        mb.Hreg(chan * 20 + 0x1008, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_PDC) * 10));
                    } else if (inv->Statistics()->getStringMaxPower(c) == 0) {
                        mb.Hreg(chan * 20 + 0x1004, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_UDC) * 10));
                        mb.Hreg(chan * 20 + 0x1005, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_IDC) * 100));
                        mb.Hreg(chan * 20 + 0x1008, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_PDC) * 10));
                    }

                    mb.Hreg(chan * 20 + 0x1006, (uint16_t)(inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC) * 10));
                    mb.Hreg(chan * 20 + 0x1007, (uint16_t)(inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_F) * 100));
                    mb.Hreg(chan * 20 + 0x1009, (uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_YD)));
                    mb.Hreg(chan * 20 + 0x100A, ((uint16_t)(((uint32_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_YT) * 1000)) >> 16)) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x100B, (((uint16_t)(inv->Statistics()->getChannelFieldValue(t, c, FLD_YT) * 1000))) & 0xFFFF);
                    mb.Hreg(chan * 20 + 0x100C, (uint16_t)(inv->Statistics()->getChannelFieldValue(TYPE_INV, CH0, FLD_T) * 10));
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
    // Fronius
    if (!(Datastore.getIsAllEnabledReachable()) || !(Datastore.getTotalAcYieldTotalEnabled() != 0) || (!_isstarted)) {
        MessageOutput.printf("Fronius SM Simulation: not updating registers! (Total Yield = 0 or not all configured inverters reachable).\r\n");
        return;
    } else {
        setHRegs(40091, Datastore.getTotalAcPowerEnabled() * -1);
        float value = (Datastore.getTotalAcYieldTotalEnabled() * 1000);
        if (value > _lasttotal) {
            _lasttotal = value;
            setHRegs(40101, value);
        }
        /*
        if (Hoymiles.getNumInverters() == 1) {
            auto inv = Hoymiles.getInverterByPos(0);
            if (inv != nullptr) {
                for (auto& t : inv->Statistics()->getChannelTypes()) {
                    if (t == TYPE_DC) {
                        setHregs(40071, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC));
                        setHregs(40073, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1));
                        setHRegs(40075, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_2));
                        setHRegs(40077, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_3));
                        setHRegs(40079, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_12));
                        setHRegs(40081, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_23));
                        setHRegs(40083, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_31));
                        setHRegs(40085, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N));
                        setHRegs(40087, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N));
                        setHRegs(40089, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_3N));
                        setHRegs(40091, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));
                        setHRegs(40093, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_F));
                        setHRegs(40095, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_Q)*inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PF));  // reactive power
                        setHRegs(40097, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_Q));  // apparent power
                        setHRegs(40099, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PF)); // Power Factor
                        setHRegs(40101, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_YT)*1000.0);  // AC Lifetime Energy Production
                        //setHRegs(40103, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_IDC) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N) * -1);
                        //setHRegs(40105, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UDC) * inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N) * -1);
                        setHRegs(40107, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PDC));
                        setHRegs(40109, inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_T));
                        // setHRegs(40111, 0.0); // Collant or Heat Sink Temperature
                        // setHRegs(40113, 0.0); // Transformer Temperature
                        // setHRegs(40115, 0.0); // other Temperature
                        // setHRegs(40123, 0.0);
                        // setHRegs(40125, 0.0);
                        // setHRegs(40127, 0.0);
                        float value = (inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_YT)*1000);
                        if (value != 0){
                            setHRegs(40129, value);
                        }
                    }
                }
            }
        } else {
            setHRegs(40091, Datastore.getTotalAcPowerEnabled() * -1);
            float value = (Datastore.getTotalAcYieldTotalEnabled() * 1000);
            if (value > _lasttotal && Datastore.getIsAllEnabledReachable()) {
                _lasttotal = value;
                setHRegs(40101, value);
            }
        }*/
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
