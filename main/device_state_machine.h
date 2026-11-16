#pragma once
#include <atomic>
#include <functional>

#include <utility>
#include <vector>

#include "device_state.h"

class DeviceStateMachine {
public:
  DeviceStateMachine() = default;
  DeviceStateMachine(const DeviceStateMachine&) = delete; // DeviceStateMachine b(a)
  DeviceStateMachine& operator=(const DeviceStateMachine&) = delete; // c = a

  DeviceState GetState() const {
    return current_state_.load();
  }

  bool CanTransitionTo(DeviceState target) const;
  bool TransitionTo(DeviceState new_state);
  using StateCallback = std::function<void(DeviceState from, DeviceState to)>;
  int AddStateChangeListener(StateCallback callback);
  void RemoveStateChangeListener(int listener_id);
  static const char* GetStateName(DeviceState state);
  

private:
  std::atomic<DeviceState> current_state_{kDeviceStateUnknown};
  std::vector<std::pair<int, StateCallback>> listeners_;
  int next_listener_id_ = 0;
  void NotifyStateChange(DeviceState from, DeviceState to);
  bool IsValidTransition(DeviceState from, DeviceState to) const;
};
