// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

class TimeoutHelper {
public:
    TimeoutHelper();
    void set(const uint32_t ms);
    uint32_t get() { return timeout; }
    void extend(const uint32_t ms);
    void reset();
    bool occured() const;
    uint32_t elapsed() const;

private:
    uint32_t startMillis;
    uint32_t timeout;
};
