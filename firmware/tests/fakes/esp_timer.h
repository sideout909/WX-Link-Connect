#pragma once
#include <cstdint>
inline int64_t fake_time_us=0;
inline int64_t esp_timer_get_time(){return fake_time_us;}
