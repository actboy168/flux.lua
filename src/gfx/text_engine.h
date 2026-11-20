#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "text/text_types.h"

namespace flux::gfx {

class text_engine {
public:
    bool init();
    void shutdown();

    bool ready() const {
        return factory_ != nullptr;
    }

    bool set_font_family(std::wstring_view family);
    bool set_font_family_utf8(std::string_view family);
    const std::wstring& font_family() const {
        return family_;
    }

    bool measure(const char* utf8, float size, float& width, float& height);
    bool measure(const char* utf8, float size, const flux::text::text_options& options,
                 float& width, float& height);
    bool measure(const char* utf8, float size, const flux::text::text_options& options,
                 float& width, float& height, int& line_count, int& overflow);
    bool caret_metrics(const char* utf8, float size, const flux::text::text_options& options,
                       int byte_offset, float& x, float& y, float& height);
    int hit_test(const char* utf8, float size, const flux::text::text_options& options, float x, float y);
    void draw(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush, const char* utf8,
              float x, float y, float size, uint32_t color);
    void draw(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush, const char* utf8,
              float x, float y, float size, uint32_t color, const flux::text::text_options& options);

private:
    IDWriteTextFormat* format(float size, int weight);
    float line_height(float size, int weight);
    IDWriteTextLayout* layout(std::string_view text, float size, const flux::text::text_options& options,
                              float& width, float& height, int* line_count = nullptr,
                              int* overflow = nullptr);
    uint64_t layout_key(std::string_view text, float size, const flux::text::text_options& options) const;

    bool family_exists(const std::wstring& family) const;
    void clear_caches();

    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::wstring family_;
    std::wstring default_family_;
    std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<IDWriteTextFormat>> formats_;
    std::unordered_map<uint64_t, float> line_heights_;
    struct layout_entry {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        float width = 0.0f;
        float height = 0.0f;
        int line_count = 0;
        int overflow = 0;
    };
    std::unordered_map<uint64_t, layout_entry> layouts_;
};

text_engine& text_engine_instance();

}
