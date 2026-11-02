#pragma once

/// @file platform_cover.h
/// @brief ESPHome cover entity for IO‑Homecontrol devices.
/// @ingroup hioc_platforms
///
/// Maps IO‑Homecontrol devices (shutters, awnings, blinds) to Home Assistant cover entities
/// with position control and real‑time feedback.
///
/// Position mapping between Home Assistant and IO‑Homecontrol:
///   HA:  1.0 = fully open,  0.0 = fully closed
///   IO:  0   = fully open,  100  = fully closed
/// The conversion is: ha_position = 1.0 - (io_position / 100.0)
///
/// @warning Some device types (e.g., horizontal awnings) have inverted position mapping
///          by default (device_feature::INVERTED). The invert
///          flag flips the HA↔IO conversion accordingly. Ensure the device's actual
///          behavior matches the mapping, or set invert_position accordingly.

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/core/preferences.h"
#include "platform_entity_base.h"

namespace esphome {
namespace home_io_control {

/// @brief Cover entity representing an IO‑Homecontrol shutter/awning/blind.
/// @ingroup hioc_platforms
///
/// The device-binding setters, setup() registration ritual and poll-interval dump line come
/// from DeviceBoundEntity; the richer position/tilt/movement mapping and the invert option
/// stay here.
class IOHomeCover : public cover::Cover, public Component, public DeviceBoundEntity {
 public:
  IOHomeCover() = default;
  /// @brief Initialize the cover entity (register device, subscribe to updates, schedule initial status poll).
  void setup() override;
  /// @brief Dump configuration to log.
  void dump_config() override;
  /// @brief Get setup priority (DATA so we have device registry available).
  /// @return setup_priority::DATA.
  [[nodiscard]] float get_setup_priority() const override { return setup_priority::DATA; }

  /// @brief Return the traits object describing this cover's capabilities.
  /// @return CoverTraits configured for position (with intermediate positions only), stop, optional
  ///         tilt, and assumed state for a cover that is only open or closed.
  cover::CoverTraits get_traits() override;

  /// @brief Enable or disable position inversion.
  /// Some devices (e.g., horizontal awnings) report reversed open/close semantics.
  /// @param invert True to invert the mapping (HA 1.0 → IO 0, HA 0.0 → IO 100).
  void set_invert_position(bool invert) {
    this->invert_ = invert;
    this->invert_explicit_ = true;
  }
  /// @brief Opt this cover into the periodic rain sensor poll (from YAML `rain_sensor_poll_interval`).
  /// @param interval_ms Poll interval in milliseconds; zero (the default) means no poll.
  void set_rain_sensor_poll_interval(uint32_t interval_ms) { this->rain_sensor_poll_interval_ms_ = interval_ms; }
  /// @brief Keep the last observed position and tilt in flash and show them again after a reboot
  ///        (from YAML `restore_position`).
  ///
  /// Without it a cover whose device answers the boot poll with a marker instead of a height (an
  /// exterior venetian blind whose last move was slat-only reports 0xD800) has no position to show
  /// until the next travel, and ESPHome cannot publish "unknown": the entity shows its default,
  /// fully open. The restored value is replaced by the first real report.
  /// @param restore True to persist and restore.
  void set_restore_position(bool restore) { this->restore_position_ = restore; }
  /// @brief Query whether this device takes intermediate positions (a slider) or only open / close / stop.
  /// @return true if the device has device_feature::LEVELS.
  [[nodiscard]] bool supports_intermediate_positions() const;
  /// @brief Query whether this device supports tilt (slat angle) control.
  /// @return true if the device has device_feature::TILT.
  [[nodiscard]] bool supports_tilt() const;

 protected:
  /// @brief Handle cover commands from Home Assistant (open/close/stop/set_position).
  /// @param call CoverCall containing the requested operation.
  void control(const cover::CoverCall &call) override;
  /// @brief Callback invoked when the underlying device state changes.
  /// @param id Device ID that updated.
  /// @param dev Updated IoDevice state.
  void on_device_update_(const std::string &id, const IoDevice &dev);
  /// @brief Resolve the current inversion mode.
  /// @return Explicit YAML override when set, otherwise the runtime device profile.
  [[nodiscard]] bool effective_invert_() const;
  /// @brief Infer HA movement direction from successive IO positions when the protocol target is unknown.
  /// @param invert Effective inversion mode for this device.
  /// @param current_io_position Latest reported IO position (0=open, 100=closed).
  /// @return OPENING, CLOSING, or IDLE when no direction can be inferred yet.
  [[nodiscard]] cover::CoverOperation infer_operation_from_position_delta_(bool invert,
                                                                           float current_io_position) const;

  /// @brief Load the persisted position/tilt (if any) and show it until the device reports one.
  void restore_persisted_state_();
  /// @brief Persist an observed resting position/tilt when it differs from what is stored.
  /// @param dev Device state just received.
  void persist_observed_state_(const IoDevice &dev);

  /// Persisted blob: IO position (0=open, 100=closed) and tilt (0=closed, 100=open), each
  /// UNKNOWN_POSITION when never observed.
  struct PersistedState {
    float io_position;
    float tilt;
  };

  bool invert_{false};
  bool restore_position_{false};
  ESPPreferenceObject pref_;
  PersistedState persisted_{UNKNOWN_POSITION, UNKNOWN_POSITION};
  bool invert_explicit_{false};
  float last_io_position_{UNKNOWN_POSITION};
  uint32_t rain_sensor_poll_interval_ms_{0};  ///< 0 = not opted in; no rain poll is scheduled.
};

}  // namespace home_io_control
}  // namespace esphome
