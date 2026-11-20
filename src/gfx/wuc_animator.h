#pragma once

#include <windows.h>
#include <windows.foundation.h>
#include <windows.ui.composition.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "gfx/layer.h"

namespace flux::gfx {

constexpr UINT k_animation_completed_message = WM_APP + 0x64;

class wuc_animator {
public:
    bool init(HWND hwnd, ABI::Windows::UI::Composition::ICompositor* compositor,
              std::string& error);
    void shutdown();

    uint64_t start(ABI::Windows::UI::Composition::IVisual* visual,
                   const flux::gfx::layer_animation_spec& spec, float dpi_scale, uint64_t id,
                   std::string& error);
    bool stop(uint64_t id);
    bool finish(uint64_t id);
    bool running(uint64_t id) const;
    int active_count() const;
    std::vector<uint64_t> cancel_all();

    static bool system_animations_enabled();

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    class completed_handler;

    struct record {
        bool infinite = false;
        ComPtr<ABI::Windows::UI::Composition::ICompositionScopedBatch> batch;
        std::vector<ComPtr<ABI::Windows::UI::Composition::ICompositionAnimation>> animations;
        ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
            IInspectable*,
            ABI::Windows::UI::Composition::CompositionBatchCompletedEventArgs*>> handler;
        EventRegistrationToken token = {};
        bool has_token = false;
        ComPtr<ABI::Windows::UI::Composition::ICompositionObject> object;
        std::vector<std::wstring> properties;
    };

    uint64_t next_id_ = 1;
    HWND hwnd_ = nullptr;
    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor_;
    ComPtr<ABI::Windows::UI::Composition::ICompositor4> compositor4_;
    std::unordered_map<uint64_t, record> records_;
};

}
