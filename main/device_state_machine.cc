#include "device_state_machine.h"

#include <algorithm>
#include <utility>
#include "esp_log.h"

static const char* TAG = "state";

static const char* const kStateNames[] = {
    "unknown",
    "starting",
    "wifi_configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "audio_testing",
    "fatal_error",
    "invalid",
};

const char* DeviceStateMachine::GetStateName(DeviceState state) {
    if (state >= kDeviceStateUnknown && state <= kDeviceStateFatalError) {
        return kStateNames[state];
    }
    return kStateNames[kDeviceStateFatalError+1];
}

bool DeviceStateMachine::IsValidTransition(DeviceState from, DeviceState to) const {
    if(from == to){
        return true;
    }

    switch(from){
        case kDeviceStateUnknown:   
            return to == kDeviceStateStarting;
        case kDeviceStateStarting:
            return to == kDeviceStateIdle;
        case kDeviceStateIdle:
            return to == kDeviceStateConnecting || to == kDeviceStateListening;
        case kDeviceStateConnecting:
            return to == kDeviceStateIdle || to == kDeviceStateListening;
        case kDeviceStateListening:
            return to == kDeviceStateIdle || to == kDeviceStateSpeaking;
        case kDeviceStateSpeaking:
            return to == kDeviceStateIdle || to == kDeviceStateListening;
        default:
            return false;
    }
}

bool DeviceStateMachine::CanTransitionTo(DeviceState target) const {
    return IsValidTransition(current_state_.load(), target);
}

bool DeviceStateMachine::TransitionTo(DeviceState new_state) {
    const DeviceState old_state = current_state_.load();
    if(old_state == new_state){
        return true;
    }
    if (!IsValidTransition(old_state, new_state)) {
        ESP_LOGW(TAG, "Invalid transition from %s to %s", GetStateName(old_state), GetStateName(new_state));
        return false;
    }
    current_state_.store(new_state);
    ESP_LOGI(TAG, "Transitioned from %s to %s", GetStateName(old_state), GetStateName(new_state));
    NotifyStateChange(old_state, new_state);
    return true;
}

int DeviceStateMachine::AddStateChangeListener(StateCallback callback) {
    const int id = next_listener_id_++;
    listeners_.emplace_back(id, std::move(callback));
    return id;
}


// 初始:   [ 1 | 2 | 3 | 4 ]
// 写指针 w=0

// 读 1 → 保留，写到 0（原地）
// 读 2 → 匹配，跳过                       ← 2 没有被"移走"，是不再被写入
// 读 3 → 保留，写到 1（覆盖掉 2 的位置）
// 读 4 → 匹配，跳过

// 返回 w = 2

// 此时容器: [ 1 | 3 | 3? | 4? ]
void DeviceStateMachine::RemoveStateChangeListener(int listener_id) {
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                    [listener_id](const auto& p) {
                                        return p.first == listener_id;
                                    }),
                     listeners_.end());
}

void DeviceStateMachine::NotifyStateChange(DeviceState from, DeviceState to) {
    for (const auto& entry : listeners_) {
        entry.second(from, to);
    }
}
