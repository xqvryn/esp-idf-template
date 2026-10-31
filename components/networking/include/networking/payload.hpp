#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace networking {

// One MQTT DATA fragment. topic and payload are valid only for the call.
// This is not a subscription and not an event bus. The handler must not call
// start, stop, or publish; those take the client lock on another task.
struct MqttPayload {
    std::string_view topic;
    std::span<const char> payload;
    int offset{0};
    int total_size{0};
};

class MqttPayloadHandler {
    public:
    virtual ~MqttPayloadHandler() = default;
    virtual void on_payload(const MqttPayload& message) noexcept = 0;
};

inline void dispatch_mqtt_payload(MqttPayloadHandler* handler,
                                  const MqttPayload& message) noexcept {
    if (handler != nullptr) {
        handler->on_payload(message);
    }
}

} // namespace networking
