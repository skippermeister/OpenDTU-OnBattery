// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_RADIO_CMT
#include "HoymilesRadio_CMT.h"
#endif
#include "HoymilesRadio_NRF.h"
#include "inverters/InverterAbstract.h"
#include "types.h"
#include <Print.h>
#include <SPI.h>
#include <memory>
#include <vector>

#define HOY_SYSTEM_CONFIG_PARA_POLL_INTERVAL (2 * 60 * 1000) // 2 minutes
#define HOY_SYSTEM_CONFIG_PARA_POLL_MIN_DURATION (4 * 60 * 1000) // at least 4 minutes between sending limit command and read request. Otherwise eventlog entry

class HoymilesClass {
public:
    void init();
    void initNRF(SPIClass* initialisedSpiBus, const uint8_t pinCE, const uint8_t pinIRQ);
#ifdef USE_RADIO_CMT
    void initCMT(const int8_t pin_sdio, const int8_t pin_clk, const int8_t pin_cs, const int8_t pin_fcs,
                 const int8_t pin_gpio2, const int8_t pin_gpio3,
                 const int8_t chip_int1gpio=2, const int8_t chip_int2gpio=3);
#endif
    void loop();

    void setMessageOutput(Print* output);
    Print* getMessageOutput();
    Print* getVerboseMessageOutput();

    std::shared_ptr<InverterAbstract> addInverter(const char* name, const uint64_t serial);
    std::shared_ptr<InverterAbstract> getInverterByPos(const uint8_t pos);
    std::shared_ptr<InverterAbstract> getInverterBySerial(const uint64_t serial);
    std::shared_ptr<InverterAbstract> getInverterByFragment(const fragment_t& fragment);
    void removeInverterBySerial(const uint64_t serial);
    size_t getNumInverters() const;

    HoymilesRadio_NRF* getRadioNrf();
#ifdef USE_RADIO_CMT
    HoymilesRadio_CMT* getRadioCmt();
#endif

    uint32_t PollInterval() const;
    void setPollInterval(const uint32_t interval);
    void setVerboseLogging(bool verboseLogging);
    bool getVerboseLogging(void) {return _verboseLogging; };

    bool isAllRadioIdle() const;

private:
    std::vector<std::shared_ptr<InverterAbstract>> _inverters;
    std::unique_ptr<HoymilesRadio_NRF> _radioNrf;
#ifdef USE_RADIO_CMT
    std::unique_ptr<HoymilesRadio_CMT> _radioCmt;
#endif

    std::mutex _mutex;

    uint32_t _pollInterval = 0;
    bool _verboseLogging = false;
    uint32_t _lastPoll = 0;

    Print* _messageOutput = &Serial;
};

extern HoymilesClass Hoymiles;
