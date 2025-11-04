#include "object_locker.h"

namespace esphome {
    namespace xt211 {

        std::vector<void *> AnyObjectLocker::locked_objects_(5);
        Mutex AnyObjectLocker::lock_;

// unlock() function removed

    }; // namespace xt211
}; // namespace esphome