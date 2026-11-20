#pragma once

#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

#include "ui/draw_list.h"

namespace flux::gfx {

class offscreen_renderer {
public:
    bool init(int logical_width, int logical_height, float dpi_scale, std::string& error);
    void shutdown();

    bool frame(const flux::ui::draw_list& draw_list, uint32_t background, float dpi_scale);
    bool save_png(const std::string& path, std::string& error);
    bool hash(uint64_t& value, std::string& error);
    uint32_t pixel(int x, int y);
    bool compare_png(const std::string& path, uint64_t& differing, int& max_diff,
                     std::string& error);

    bool ready() const {
        return target_ != nullptr;
    }

    int logical_width() const {
        return logical_width_;
    }
    int logical_height() const {
        return logical_height_;
    }
    int pixel_width() const {
        return pixel_width_;
    }
    int pixel_height() const {
        return pixel_height_;
    }
    float dpi_scale() const {
        return dpi_scale_;
    }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<IWICImagingFactory> wic_;
    ComPtr<IWICBitmap> bitmap_;
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1RenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;

    int logical_width_ = 0;
    int logical_height_ = 0;
    int pixel_width_ = 0;
    int pixel_height_ = 0;
    float dpi_scale_ = 1.0f;
    bool com_initialized_ = false;
};

}
