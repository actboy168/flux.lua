#include "gfx/offscreen_renderer.h"

#include <d2d1helper.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <filesystem>

#include "text/utf8.h"
#include "gfx/draw_replay.h"
#include "platform/utf8_win32.h"

namespace flux::gfx {

namespace {

bool fail(std::string& error, const char* what, HRESULT hr) {
    error = std::format("{} failed: 0x{:08X}", what, static_cast<uint32_t>(hr));
    return false;
}

}

bool offscreen_renderer::init(int logical_width, int logical_height, float dpi_scale,
                              std::string& error) {
    shutdown();

    if (logical_width <= 0 || logical_height <= 0 || dpi_scale <= 0.0f) {
        error = "invalid offscreen size";
        return false;
    }

    logical_width_ = logical_width;
    logical_height_ = logical_height;
    dpi_scale_ = dpi_scale;
    pixel_width_ = std::max(1, static_cast<int>(std::lround(logical_width * dpi_scale)));
    pixel_height_ = std::max(1, static_cast<int>(std::lround(logical_height * dpi_scale)));

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        com_initialized_ = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        error = "CoInitializeEx failed";
        return false;
    }

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wic_.GetAddressOf()));
    if (FAILED(hr)) {
        shutdown();
        return fail(error, "CoCreateInstance(WICImagingFactory)", hr);
    }

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
    if (FAILED(hr)) {
        shutdown();
        return fail(error, "D2D1CreateFactory", hr);
    }

    hr = wic_->CreateBitmap(
        static_cast<UINT>(pixel_width_),
        static_cast<UINT>(pixel_height_),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        bitmap_.GetAddressOf());
    if (FAILED(hr)) {
        shutdown();
        return fail(error, "IWICImagingFactory::CreateBitmap", hr);
    }

    D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    hr = d2d_factory_->CreateWicBitmapRenderTarget(
        bitmap_.Get(),
        &properties,
        target_.GetAddressOf());
    if (FAILED(hr)) {
        shutdown();
        return fail(error, "ID2D1Factory::CreateWicBitmapRenderTarget", hr);
    }

    target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    hr = target_->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        brush_.GetAddressOf());
    if (FAILED(hr)) {
        shutdown();
        return fail(error, "ID2D1RenderTarget::CreateSolidColorBrush", hr);
    }
    return true;
}

void offscreen_renderer::shutdown() {
    brush_.Reset();
    target_.Reset();
    bitmap_.Reset();
    d2d_factory_.Reset();
    wic_.Reset();
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
    logical_width_ = 0;
    logical_height_ = 0;
    pixel_width_ = 0;
    pixel_height_ = 0;
    dpi_scale_ = 1.0f;
}

bool offscreen_renderer::frame(const flux::ui::draw_list& draw_list, uint32_t background, float dpi_scale) {
    if (!target_ || !brush_) {
        return false;
    }
    const float scale = dpi_scale > 0.0f ? dpi_scale : dpi_scale_;
    target_->BeginDraw();
    replay_draw_list(target_.Get(), brush_.Get(), draw_list, background, scale);
    HRESULT hr = target_->EndDraw();
    return SUCCEEDED(hr);
}

bool offscreen_renderer::save_png(const std::string& path, std::string& error) {
    if (!wic_ || !bitmap_) {
        error = "offscreen renderer is not initialized";
        return false;
    }
    std::filesystem::path file(path);
    if (file.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
    }

    const std::wstring wide_path = flux::platform::utf8_to_wide(path);
    Microsoft::WRL::ComPtr<IWICStream> stream;
    HRESULT hr = wic_->CreateStream(stream.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICImagingFactory::CreateStream", hr);
    hr = stream->InitializeFromFilename(wide_path.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return fail(error, "IWICStream::InitializeFromFilename", hr);

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    hr = wic_->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICImagingFactory::CreateEncoder", hr);
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return fail(error, "IWICBitmapEncoder::Initialize", hr);

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> properties;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICBitmapEncoder::CreateNewFrame", hr);
    hr = frame->Initialize(properties.Get());
    if (FAILED(hr)) return fail(error, "IWICBitmapFrameEncode::Initialize", hr);

    hr = frame->SetSize(static_cast<UINT>(pixel_width_), static_cast<UINT>(pixel_height_));
    if (FAILED(hr)) return fail(error, "IWICBitmapFrameEncode::SetSize", hr);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) return fail(error, "IWICBitmapFrameEncode::SetPixelFormat", hr);
    hr = frame->WriteSource(bitmap_.Get(), nullptr);
    if (FAILED(hr)) return fail(error, "IWICBitmapFrameEncode::WriteSource", hr);
    hr = frame->Commit();
    if (FAILED(hr)) return fail(error, "IWICBitmapFrameEncode::Commit", hr);
    hr = encoder->Commit();
    if (FAILED(hr)) return fail(error, "IWICBitmapEncoder::Commit", hr);
    return true;
}

bool offscreen_renderer::hash(uint64_t& value, std::string& error) {
    if (!bitmap_) {
        error = "offscreen renderer is not initialized";
        return false;
    }
    WICRect rect = {0, 0, pixel_width_, pixel_height_};
    Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
    HRESULT hr = bitmap_->Lock(&rect, WICBitmapLockRead, lock.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICBitmap::Lock", hr);

    UINT stride = 0;
    UINT size = 0;
    BYTE* data = nullptr;
    hr = lock->GetStride(&stride);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetStride", hr);
    hr = lock->GetDataPointer(&size, &data);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetDataPointer", hr);

    uint64_t result = 1469598103934665603ull;
    for (UINT i = 0; i < size; ++i) {
        result ^= data[i];
        result *= 1099511628211ull;
    }
    value = result;
    return true;
}

uint32_t offscreen_renderer::pixel(int x, int y) {
    if (!bitmap_ || x < 0 || y < 0 || x >= pixel_width_ || y >= pixel_height_) {
        return 0;
    }
    WICRect rect = {x, y, 1, 1};
    Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
    if (FAILED(bitmap_->Lock(&rect, WICBitmapLockRead, lock.GetAddressOf()))) {
        return 0;
    }
    UINT stride = 0;
    UINT size = 0;
    BYTE* data = nullptr;
    if (FAILED(lock->GetStride(&stride)) || FAILED(lock->GetDataPointer(&size, &data))) {
        return 0;
    }
    const uint8_t* p = data;
    return (static_cast<uint32_t>(p[3]) << 24)
        | (static_cast<uint32_t>(p[0]) << 16)
        | (static_cast<uint32_t>(p[1]) << 8)
        | static_cast<uint32_t>(p[2]);
}

bool offscreen_renderer::compare_png(const std::string& path, uint64_t& differing,
                                    int& max_diff, std::string& error) {
    differing = 0;
    max_diff = 0;
    if (!wic_ || !bitmap_) {
        error = "offscreen renderer is not initialized";
        return false;
    }

    const std::wstring wide_path = flux::platform::utf8_to_wide(path);
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wic_->CreateDecoderFromFilename(
        wide_path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr)) return fail(error, "WIC decoder open", hr);

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return fail(error, "WIC decoder GetFrame", hr);

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wic_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICImagingFactory::CreateFormatConverter", hr);
    hr = converter->Initialize(
        frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return fail(error, "IWICFormatConverter::Initialize", hr);

    Microsoft::WRL::ComPtr<IWICBitmap> golden;
    hr = wic_->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, golden.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICImagingFactory::CreateBitmapFromSource", hr);

    UINT golden_width = 0;
    UINT golden_height = 0;
    hr = golden->GetSize(&golden_width, &golden_height);
    if (FAILED(hr)) return fail(error, "IWICBitmap::GetSize", hr);
    if (golden_width != static_cast<UINT>(pixel_width_)
        || golden_height != static_cast<UINT>(pixel_height_)) {
        error = std::format("image size mismatch: golden {}x{}, current {}x{}",
                            golden_width, golden_height, pixel_width_, pixel_height_);
        return false;
    }

    WICRect rect = {0, 0, pixel_width_, pixel_height_};
    Microsoft::WRL::ComPtr<IWICBitmapLock> current_lock;
    Microsoft::WRL::ComPtr<IWICBitmapLock> golden_lock;
    hr = bitmap_->Lock(&rect, WICBitmapLockRead, current_lock.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICBitmap::Lock(current)", hr);
    hr = golden->Lock(&rect, WICBitmapLockRead, golden_lock.GetAddressOf());
    if (FAILED(hr)) return fail(error, "IWICBitmap::Lock(golden)", hr);

    UINT current_stride = 0;
    UINT current_size = 0;
    BYTE* current_data = nullptr;
    UINT golden_stride = 0;
    UINT golden_size = 0;
    BYTE* golden_data = nullptr;
    hr = current_lock->GetStride(&current_stride);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetStride(current)", hr);
    hr = current_lock->GetDataPointer(&current_size, &current_data);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetDataPointer(current)", hr);
    hr = golden_lock->GetStride(&golden_stride);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetStride(golden)", hr);
    hr = golden_lock->GetDataPointer(&golden_size, &golden_data);
    if (FAILED(hr)) return fail(error, "IWICBitmapLock::GetDataPointer(golden)", hr);

    for (int y = 0; y < pixel_height_; ++y) {
        const uint8_t* a = current_data + static_cast<size_t>(y) * current_stride;
        const uint8_t* b = golden_data + static_cast<size_t>(y) * golden_stride;
        for (int x = 0; x < pixel_width_; ++x) {
            int diff = 0;
            for (int c = 0; c < 4; ++c) {
                diff = std::max(diff, std::abs(static_cast<int>(a[x * 4 + c])
                                               - static_cast<int>(b[x * 4 + c])));
            }
            if (diff > 0) {
                differing += 1;
                max_diff = std::max(max_diff, diff);
            }
        }
    }
    return true;
}

}
