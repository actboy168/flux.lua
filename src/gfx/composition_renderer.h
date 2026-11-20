#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <windows.foundation.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "gfx/backdrop.h"
#include "ui/draw_list.h"
#include "gfx/layer.h"
#include "gfx/wuc_animator.h"

namespace flux::gfx {

class composition_renderer {
public:
    bool init(HWND hwnd, int client_width, int client_height, float dpi_scale,
              backdrop_mode backdrop, std::string& error);
    bool recover(std::string& error);
    void shutdown();
    bool resize(int client_width, int client_height, float dpi_scale);
    bool frame(const flux::ui::draw_list& draw_list, uint32_t background, float dpi_scale);

    bool ensure_layer(flux::ui::render_layer layer, std::string& error);
    bool layer_exists(flux::ui::render_layer layer) const;
    bool set_layer_transform(flux::ui::render_layer layer, const flux::gfx::layer_transform& transform,
                             std::string& error);
    const flux::gfx::layer_transform& layer_transform(flux::ui::render_layer layer) const;
    float dpi_scale() const {
        return dpi_scale_;
    }

    uint64_t start_animation(flux::ui::render_layer layer, const flux::gfx::layer_animation_spec& animation,
                             std::string& error);
    bool stop_animation(uint64_t id, bool apply_final, std::string& error);
    bool animation_running(uint64_t id) const;
    std::vector<uint64_t> poll_animations();
    void on_animation_completed(uint64_t id);
    bool animations_enabled() const;
    bool animations_running() const {
        return !pending_animations_.empty();
    }
    bool vsync() const {
        return present_interval_ != 0;
    }
    void set_vsync(bool enabled) {
        present_interval_ = enabled ? 1 : 0;
    }
    void debug_set_animations_enabled(int value);

    bool set_layer_blur(flux::ui::render_layer layer, float blur, std::string& error);
    float layer_blur(flux::ui::render_layer layer) const;
    bool set_layer_shadow(flux::ui::render_layer layer, const flux::gfx::layer_shadow& shadow, std::string& error);
    bool clear_layer_shadow(flux::ui::render_layer layer);
    bool layer_has_shadow(flux::ui::render_layer layer) const;
    bool set_layer_clip(flux::ui::render_layer layer, const flux::gfx::layer_clip& clip, std::string& error);
    bool clear_layer_clip(flux::ui::render_layer layer);
    bool layer_has_clip(flux::ui::render_layer layer) const;

    backdrop_mode backdrop() const {
        return backdrop_;
    }

    bool ready() const {
        return surfaces_[static_cast<int>(flux::ui::render_layer::content)].render_context != nullptr;
    }

    bool backdrop_native() const {
        return backdrop_native_;
    }

    bool software() const {
        return software_;
    }

    const std::string& last_error() const {
        return last_error_;
    }

    int pixel_width() const {
        return client_width_;
    }

    int pixel_height() const {
        return client_height_;
    }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct surface {
        flux::ui::render_layer layer = flux::ui::render_layer::content;
        bool created = false;
        ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> sprite;
        ComPtr<ABI::Windows::UI::Composition::IVisual> visual;
        ComPtr<IDXGISwapChain1> swap_chain;
        ComPtr<ABI::Windows::UI::Composition::ICompositionSurface> composition_surface;
        ComPtr<ID2D1DeviceContext> render_context;
        ComPtr<ID2D1Bitmap1> target_bitmap;
        ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> brush;
        ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> effect_brush;
        float blur = 0.0f;
        bool has_shadow = false;
        flux::gfx::layer_shadow shadow;
        ComPtr<ABI::Windows::UI::Composition::IDropShadow> drop_shadow;
        bool has_clip = false;
        flux::gfx::layer_clip clip;
        ComPtr<ABI::Windows::UI::Composition::ICompositionRoundedRectangleGeometry> clip_geometry;
        ComPtr<ABI::Windows::UI::Composition::ICompositionGeometricClip> geometric_clip;
    };

    bool init_device(std::string& error);
    bool init_compositor(std::string& error);
    bool create_layer(flux::ui::render_layer layer, std::string& error);
    bool create_swap_chain(surface& layer, std::string& error);
    bool create_render_target(surface& layer, std::string& error);
    void destroy_layer(flux::ui::render_layer layer);
    void destroy_layers();
    bool create_backdrop(std::string& error);
    bool insert_layer_visual(flux::ui::render_layer layer);
    bool remove_layer_visual(flux::ui::render_layer layer);
    void apply_layer_transform(flux::ui::render_layer layer);
    void apply_layer_property(flux::ui::render_layer layer, flux::gfx::layer_property property);
    bool apply_layer_effect(flux::ui::render_layer layer, std::string& error);
    bool apply_layer_shadow(flux::ui::render_layer layer, std::string& error);
    bool apply_layer_clip(flux::ui::render_layer layer, std::string& error);
    void apply_track_value(flux::ui::render_layer layer, const flux::gfx::layer_animation_track& track,
                           const flux::gfx::layer_value& value);
    void cancel_animation(uint64_t id, bool apply_final);
    void cancel_layer_animations(flux::ui::render_layer layer, bool apply_final);
    void cancel_all_animations(bool apply_final);
    bool render_content_all(const flux::ui::draw_list& draw_list, uint32_t background, float scale);
    ABI::Windows::Foundation::Numerics::Vector2 visual_size() const;
    void update_visual_sizes();

    struct pending_animation {
        flux::ui::render_layer layer = flux::ui::render_layer::content;
        flux::gfx::layer_animation_spec spec;
    };

    HWND hwnd_ = nullptr;
    int client_width_ = 0;
    int client_height_ = 0;
    float dpi_scale_ = 1.0f;
    backdrop_mode backdrop_ = backdrop_mode::none;
    bool backdrop_native_ = false;
    bool software_ = false;
    bool ole_initialized_ = false;
    std::string last_error_;

    ComPtr<IUnknown> dispatcher_controller_;
    ComPtr<ID3D11Device> d3d_device_;
    ComPtr<ID3D11DeviceContext> d3d_context_;
    ComPtr<IDXGIDevice> dxgi_device_;
    ComPtr<ID2D1Factory1> d2d_factory_;
    ComPtr<ID2D1Device> d2d_device_;

    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor_;
    ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop_;
    ComPtr<ABI::Windows::UI::Composition::ICompositor2> compositor2_;
    ComPtr<ABI::Windows::UI::Composition::ICompositor5> compositor5_;
    ComPtr<ABI::Windows::UI::Composition::ICompositor6> compositor6_;
    ComPtr<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop> desktop_interop_;
    ComPtr<ABI::Windows::UI::Composition::ICompositionTarget> target_;
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> root_container_;
    ComPtr<ABI::Windows::UI::Composition::IVisual> root_visual_;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> backdrop_sprite_;
    ComPtr<ABI::Windows::UI::Composition::IVisual> backdrop_visual_;
    std::array<surface, flux::ui::k_render_layer_count> surfaces_;
    std::array<flux::gfx::layer_transform, flux::ui::k_render_layer_count> base_transforms_;
    wuc_animator animator_;
    std::unordered_map<uint64_t, pending_animation> pending_animations_;
    std::vector<uint64_t> completed_animations_;
    uint64_t next_animation_id_ = 1;
    int animations_override_ = -1;
    UINT present_interval_ = 1;
};

}
