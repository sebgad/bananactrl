#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "banana/config/Config.hpp"
#include "banana/control/ControlLoop.hpp"

/// Home Assistant MQTT integration, pure logic (host-tested): topics, discovery messages, state payload and
/// command handling. The MQTT client itself is MqttService.
///
/// Topics, with <id> = device id (e.g. "bananactrl_3068b0"):
///   bananactrl/<id>/availability   "online" / "offline" (retained, offline is the last will)
///   bananactrl/<id>/state          JSON, every few seconds and after each command
///   bananactrl/<id>/<command>/set  commands from Home Assistant (target, standby_time, restart)
///   homeassistant/<component>/<id>/<entity>/config   discovery (retained)
namespace banana::mqtt {

inline constexpr std::string_view kDiscoveryPrefix = "homeassistant";
inline constexpr std::string_view kHomeAssistantStatusTopic =
    "homeassistant/status"; ///< "online" after a HA restart

/// Limits for the controls, enforced on the device (Home Assistant's min/max are only UI hints).
inline constexpr float kTargetMin = 60.0F;
inline constexpr float kTargetMax = 100.0F;
inline constexpr float kTargetStep = 0.5F;
inline constexpr int kStandbyMinMinutes = 10;
inline constexpr int kStandbyMaxMinutes = 600;

struct Device {
    std::string id;                    ///< unique, used in topics and unique_ids
    std::string name = "BananaCoffee"; ///< device name in Home Assistant
    std::string version;               ///< firmware version
    std::string url;                   ///< web UI, shown as "Visit" link
};

enum class Command : std::uint8_t {
    Target,      ///< number: target temperature [°C]
    StandbyTime, ///< number: time to standby [min]
    Restart,     ///< button
};

struct Message {
    std::string topic;
    std::string payload;
};

class Topics {
public:
    explicit Topics(std::string_view deviceId) : base_("bananactrl/" + std::string{deviceId}) {}

    [[nodiscard]] std::string availability() const { return base_ + "/availability"; }
    [[nodiscard]] std::string state() const { return base_ + "/state"; }
    [[nodiscard]] std::string command(Command command) const;
    /// The command a received topic belongs to.
    [[nodiscard]] std::optional<Command> commandFor(std::string_view topic) const;

private:
    std::string base_;
};

/// One retained config message per entity.
[[nodiscard]] std::vector<Message> discoveryMessages(const Device& device, const Topics& topics);

/// State JSON for all entities. `wifiPercent`: station signal 0..100 %.
[[nodiscard]] std::string statePayload(const control::ProcessSnapshot& snapshot, const config::Config& config,
                                       int wifiPercent);

/// Applies a number command (payload as sent by Home Assistant, e.g. "92.5") to `config`, clamped to the
/// limits above. False if the payload is not a number or the command is not a setting.
[[nodiscard]] bool applyCommand(Command command, std::string_view payload, config::Config& config);

} // namespace banana::mqtt
