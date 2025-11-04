#pragma once
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <vector>

namespace esphome {
    namespace xt211 {

        class AnyObjectLocker {
        public:
            static bool try_lock(void *obj) {
                if (!lock_.try_lock()) {
                    return false;
                }
                bool result = false;
                if (std::find(locked_objects_.begin(), locked_objects_.end(), obj) == locked_objects_.end()) {
                    locked_objects_.push_back(obj);
                    result = true;
                }
                lock_.unlock();
                return result;
            }

            // unlock() function removed to enforce read-only/no-session-release

        private:
            static std::vector<void *> locked_objects_;
            static Mutex lock_;
        };
    }; // namespace xt211
}; // namespace esphome
