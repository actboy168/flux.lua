#include "gfx/text_engine.h"

#include <windows.h>

#include <d2d1helper.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "text/utf8.h"
#include "gfx/color.h"
#include "platform/utf8_win32.h"

namespace flux::gfx {

namespace {

uint64_t mix(uint64_t hash, uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
    return hash;
}

std::wstring system_ui_font() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)
        && metrics.lfMessageFont.lfFaceName[0] != L'\0') {
        return metrics.lfMessageFont.lfFaceName;
    }
    LOGFONTW font = {};
    if (GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(font), &font)
        && font.lfFaceName[0] != L'\0') {
        return font.lfFaceName;
    }
    return {};
}

}

text_engine& text_engine_instance() {
    static text_engine engine;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        engine.init();
    }
    return engine;
}

bool text_engine::init() {
    if (factory_) {
        return true;
    }
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory_.GetAddressOf()));
    if (FAILED(hr) || !factory_) {
        return false;
    }

    default_family_ = system_ui_font();
    if (family_.empty()) {
        family_ = default_family_;
    }
    return true;
}

void text_engine::shutdown() {
    clear_caches();
    factory_.Reset();
    family_.clear();
    default_family_.clear();
}

void text_engine::clear_caches() {
    formats_.clear();
    line_heights_.clear();
    layouts_.clear();
}

bool text_engine::family_exists(const std::wstring& family) const {
    if (!factory_ || family.empty()) {
        return false;
    }
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(factory_->GetSystemFontCollection(fonts.GetAddressOf(), FALSE))) {
        return false;
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(fonts->FindFamilyName(family.c_str(), &index, &exists)) && exists;
}

bool text_engine::set_font_family(std::wstring_view family) {
    if (!init()) {
        return false;
    }
    const std::wstring requested(family);
    if (requested.empty()) {
        family_ = default_family_;
        clear_caches();
        return true;
    }
    if (!family_exists(requested)) {
        family_ = default_family_;
        clear_caches();
        return false;
    }
    family_ = requested;
    clear_caches();
    return true;
}

bool text_engine::set_font_family_utf8(std::string_view family) {
    return set_font_family(flux::platform::utf8_to_wide(family));
}

IDWriteTextFormat* text_engine::format(float size, int weight) {
    if (!factory_) {
        return nullptr;
    }
    uint64_t key = (static_cast<uint64_t>(size * 100.0f + 0.5f) << 8)
        | static_cast<uint64_t>(weight & 0xFF);
    auto it = formats_.find(key);
    if (it != formats_.end()) {
        return it->second.Get();
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> text_format;
    HRESULT hr = factory_->CreateTextFormat(
        family_.c_str(),
        nullptr,
        static_cast<DWRITE_FONT_WEIGHT>(weight),
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"zh-CN",
        text_format.GetAddressOf());
    if (FAILED(hr) && !default_family_.empty() && family_ != default_family_) {
        hr = factory_->CreateTextFormat(
            default_family_.c_str(),
            nullptr,
            static_cast<DWRITE_FONT_WEIGHT>(weight),
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"zh-CN",
            text_format.GetAddressOf());
    }
    if (FAILED(hr)) {
        return nullptr;
    }

    text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    IDWriteTextFormat* raw = text_format.Get();
    formats_.emplace(key, std::move(text_format));
    return raw;
}

float text_engine::line_height(float size, int weight) {
    uint64_t key = (static_cast<uint64_t>(size * 100.0f + 0.5f) << 8)
        | static_cast<uint64_t>(weight & 0xFF);
    auto it = line_heights_.find(key);
    if (it != line_heights_.end()) {
        return it->second;
    }
    IDWriteTextFormat* text_format = format(size, weight);
    if (text_format == nullptr) {
        return size * 1.2f;
    }
    Microsoft::WRL::ComPtr<IDWriteTextLayout> probe;
    HRESULT hr = factory_->CreateTextLayout(L"Ag", 2, text_format, 1e6f, 1e6f, probe.GetAddressOf());
    if (FAILED(hr)) {
        return size * 1.2f;
    }
    DWRITE_TEXT_METRICS metrics = {};
    hr = probe->GetMetrics(&metrics);
    float height = SUCCEEDED(hr) && metrics.height > 0.0f ? metrics.height : size * 1.2f;
    line_heights_.emplace(key, height);
    return height;
}

uint64_t text_engine::layout_key(std::string_view text, float size, const flux::text::text_options& options) const {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : text) {
        hash = mix(hash, c);
    }
    hash = mix(hash, text.size());
    hash = mix(hash, static_cast<uint64_t>(size * 100.0f + 0.5f));
    hash = mix(hash, static_cast<uint64_t>(options.width * 100.0f + 0.5f));
    hash = mix(hash, static_cast<uint64_t>(options.height * 100.0f + 0.5f));
    hash = mix(hash, static_cast<uint64_t>(options.line_height * 100.0f + 0.5f));
    hash = mix(hash, static_cast<uint64_t>(options.max_lines & 0xFFFF));
    hash = mix(hash, static_cast<uint64_t>(options.weight & 0xFFFF));
    hash = mix(hash, static_cast<uint64_t>(options.align & 0xFF));
    hash = mix(hash, static_cast<uint64_t>(options.valign & 0xFF));
    hash = mix(hash, options.wrap ? 1 : 0);
    hash = mix(hash, options.ellipsis ? 1 : 0);
    return hash;
}

IDWriteTextLayout* text_engine::layout(std::string_view text, float size, const flux::text::text_options& options,
                                      float& width, float& height, int* line_count, int* overflow) {
    if (!init()) {
        width = 0.0f;
        height = 0.0f;
        if (line_count != nullptr) *line_count = 0;
        if (overflow != nullptr) *overflow = 0;
        return nullptr;
    }

    const uint64_t key = layout_key(text, size, options);
    auto found = layouts_.find(key);
    if (found != layouts_.end()) {
        width = found->second.width;
        height = found->second.height;
        if (line_count != nullptr) *line_count = found->second.line_count;
        if (overflow != nullptr) *overflow = found->second.overflow;
        return found->second.layout.Get();
    }

    IDWriteTextFormat* text_format = format(size, options.weight);
    if (text_format == nullptr) {
        width = 0.0f;
        height = 0.0f;
        if (line_count != nullptr) *line_count = 0;
        if (overflow != nullptr) *overflow = 0;
        return nullptr;
    }

    const std::wstring wide = flux::platform::utf8_to_wide(text);
    const float max_width = options.width > 0.0f ? options.width : 1e6f;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    HRESULT hr = factory_->CreateTextLayout(
        wide.c_str(),
        static_cast<UINT32>(wide.size()),
        text_format,
        max_width,
        1e6f,
        text_layout.GetAddressOf());
    if (FAILED(hr)) {
        width = 0.0f;
        height = 0.0f;
        return nullptr;
    }

    text_layout->SetWordWrapping(options.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    text_layout->SetTextAlignment(
        options.align == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER
        : options.align == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING
        : DWRITE_TEXT_ALIGNMENT_LEADING);
    text_layout->SetParagraphAlignment(
        options.valign == 1 ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        : options.valign == 2 ? DWRITE_PARAGRAPH_ALIGNMENT_FAR
        : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    if (options.line_height > 0.0f) {
        text_layout->SetLineSpacing(
            DWRITE_LINE_SPACING_METHOD_UNIFORM,
            options.line_height,
            options.line_height * 0.8f);
    }

    float max_height = 1e6f;
    if (options.max_lines > 0) {
        const float line_size = options.line_height > 0.0f
            ? options.line_height
            : line_height(size, options.weight);
        max_height = std::min(max_height, line_size * options.max_lines);
    }
    if (options.height > 0.0f) {
        max_height = std::min(max_height, options.height);
    }
    if (max_height < 1e6f) {
        text_layout->SetMaxHeight(max_height);
    }

    if (options.ellipsis) {
        Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
        if (SUCCEEDED(factory_->CreateEllipsisTrimmingSign(text_format, sign.GetAddressOf()))) {
            DWRITE_TRIMMING trimming = {};
            trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            trimming.delimiter = 0;
            trimming.delimiterCount = 0;
            text_layout->SetTrimming(&trimming, sign.Get());
        }
    }

    DWRITE_TEXT_METRICS metrics = {};
    hr = text_layout->GetMetrics(&metrics);
    width = SUCCEEDED(hr) ? metrics.width : 0.0f;
    height = SUCCEEDED(hr) ? metrics.height : 0.0f;

    int total_lines = SUCCEEDED(hr) ? static_cast<int>(metrics.lineCount) : 0;
    int visible_lines = total_lines;
    if (options.ellipsis && total_lines > 0) {
        std::vector<DWRITE_LINE_METRICS> line_metrics(static_cast<size_t>(total_lines));
        UINT32 actual = 0;
        if (SUCCEEDED(text_layout->GetLineMetrics(
                line_metrics.data(), static_cast<UINT32>(line_metrics.size()), &actual))) {
            visible_lines = 0;
            float used_height = 0.0f;
            for (UINT32 i = 0; i < actual; ++i) {
                const float line = line_metrics[i].height;
                if (used_height + line > max_height + 0.01f) {
                    break;
                }
                used_height += line;
                if (line > 0.0f) {
                    ++visible_lines;
                }
            }
            if (max_height >= 1e6f) {
                visible_lines = static_cast<int>(actual);
            }
        }
    }
    const int overflow_lines = std::max(0, total_lines - visible_lines);

    if (layouts_.size() > 4096) {
        layouts_.clear();
    }
    layout_entry entry;
    entry.width = width;
    entry.height = height;
    entry.line_count = total_lines;
    entry.overflow = overflow_lines;
    entry.layout = text_layout;
    layouts_.emplace(key, std::move(entry));
    if (line_count != nullptr) *line_count = total_lines;
    if (overflow != nullptr) *overflow = overflow_lines;
    return text_layout.Get();
}

bool text_engine::measure(const char* utf8, float size, float& width, float& height) {
    flux::text::text_options options;
    return measure(utf8, size, options, width, height);
}

bool text_engine::measure(const char* utf8, float size, const flux::text::text_options& options,
                         float& width, float& height) {
    int line_count = 0;
    int overflow = 0;
    return measure(utf8, size, options, width, height, line_count, overflow);
}

bool text_engine::measure(const char* utf8, float size, const flux::text::text_options& options,
                         float& width, float& height, int& line_count, int& overflow) {
    width = 0.0f;
    height = 0.0f;
    line_count = 0;
    overflow = 0;
    IDWriteTextLayout* text_layout = layout(utf8 == nullptr ? "" : utf8, size, options,
                                            width, height, &line_count, &overflow);
    return text_layout != nullptr;
}

bool text_engine::caret_metrics(const char* utf8, float size, const flux::text::text_options& options,
                               int byte_offset, float& x, float& y, float& height) {
    x = 0.0f;
    y = 0.0f;
    height = line_height(size, options.weight);
    const std::string_view text = utf8 != nullptr ? utf8 : "";
    if (text.empty()) {
        return true;
    }
    float width = 0.0f;
    float measured_height = 0.0f;
    IDWriteTextLayout* text_layout = layout(text, size, options, width, measured_height);
    if (text_layout == nullptr) {
        return false;
    }
    const size_t offset = std::min(static_cast<size_t>(std::max(byte_offset, 0)), text.size());
    const UINT32 position = static_cast<UINT32>(flux::text::utf8_to_utf16_offset(text, offset));
    FLOAT caret_x = 0.0f;
    FLOAT caret_y = 0.0f;
    DWRITE_HIT_TEST_METRICS metrics = {};
    HRESULT hr = text_layout->HitTestTextPosition(
        position, FALSE, &caret_x, &caret_y, &metrics);
    if (FAILED(hr)) {
        return false;
    }
    x = caret_x;
    y = caret_y;
    height = metrics.height > 0.0f ? metrics.height : height;
    return true;
}

int text_engine::hit_test(const char* utf8, float size, const flux::text::text_options& options,
                         float x, float y) {
    const std::string_view text = utf8 != nullptr ? utf8 : "";
    if (text.empty()) {
        return 0;
    }
    float width = 0.0f;
    float height = 0.0f;
    IDWriteTextLayout* text_layout = layout(text, size, options, width, height);
    if (text_layout == nullptr) {
        return 0;
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics = {};
    HRESULT hr = text_layout->HitTestPoint(x, y, &trailing, &inside, &metrics);
    if (FAILED(hr)) {
        return 0;
    }
    size_t position = metrics.textPosition;
    if (trailing && position < flux::text::utf8_to_utf16_offset(text, text.size())) {
        position += metrics.length;
    }
    return static_cast<int>(flux::text::utf16_to_utf8_offset(text, position));
}

void text_engine::draw(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush, const char* utf8,
                      float x, float y, float size, uint32_t color) {
    flux::text::text_options options;
    draw(target, brush, utf8, x, y, size, color, options);
}

void text_engine::draw(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush, const char* utf8,
                      float x, float y, float size, uint32_t color, const flux::text::text_options& options) {
    if (target == nullptr || brush == nullptr) {
        return;
    }
    float width = 0.0f;
    float height = 0.0f;
    IDWriteTextLayout* text_layout = layout(utf8 == nullptr ? "" : utf8, size, options, width, height);
    if (text_layout == nullptr) {
        return;
    }
    brush->SetColor(color_from_u32(color));
    target->DrawTextLayout(D2D1::Point2F(x, y), text_layout, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
}

}
