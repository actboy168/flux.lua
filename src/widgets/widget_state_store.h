#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include "ui/types.h"

namespace flux::widgets {

enum class widget_state_kind : uint32_t {
    combo = 1,
    edit,
    progress,
    popup,
    list,
};

constexpr uint64_t widget_state_ttl_frames = 600;

template <typename T, typename = void>
struct widget_state_persistent : std::false_type {};

template <typename T>
struct widget_state_persistent<T, std::void_t<decltype(T::persistent)>>
    : std::bool_constant<T::persistent> {};

struct widget_state_slot_base {
    virtual ~widget_state_slot_base() = default;
    virtual widget_state_kind kind() const = 0;
    uint64_t last_frame = 0;
    bool persistent = false;
};

template <typename T>
struct widget_state_slot final : widget_state_slot_base {
    T value;
    widget_state_kind kind() const override {
        return T::kind;
    }
};

// Type-erased, per-flux::ui::ui_id storage for cross-frame widget state.
class widget_state_store {
public:
    template <typename T>
    T& get(flux::ui::ui_id id, uint64_t frame) {
        auto& slot = slots_[id];
        if (!slot || slot->kind() != T::kind) {
            slot = std::make_unique<widget_state_slot<T>>();
            slot->persistent = widget_state_persistent<T>::value;
        }
        slot->last_frame = frame;
        return static_cast<widget_state_slot<T>*>(slot.get())->value;
    }

    template <typename T>
    T* find(flux::ui::ui_id id) {
        auto it = slots_.find(id);
        if (it == slots_.end() || it->second->kind() != T::kind) {
            return nullptr;
        }
        return &static_cast<widget_state_slot<T>*>(it->second.get())->value;
    }

    template <typename T>
    const T* find(flux::ui::ui_id id) const {
        auto it = slots_.find(id);
        if (it == slots_.end() || it->second->kind() != T::kind) {
            return nullptr;
        }
        return &static_cast<const widget_state_slot<T>*>(it->second.get())->value;
    }

    void prune(uint64_t frame) {
        for (auto it = slots_.begin(); it != slots_.end();) {
            if (!it->second->persistent
                && frame > it->second->last_frame + widget_state_ttl_frames) {
                it = slots_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clear() {
        slots_.clear();
    }

private:
    std::unordered_map<flux::ui::ui_id, std::unique_ptr<widget_state_slot_base>> slots_;
};

}
