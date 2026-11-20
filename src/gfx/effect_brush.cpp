#include "gfx/effect_brush.h"

#include <d2d1effects.h>
#include <roapi.h>
#include <winstring.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include <format>
#include <cstring>
#include <cwchar>
#include <string>

namespace flux::gfx {

namespace {

constexpr GUID kgaussian_blur_effect = {
    0x1feb6d69, 0x2fe6, 0x4ac9,
    {0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5}};

using ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
using ABI::Windows::Graphics::Effects::IGraphicsEffect;
using ABI::Windows::Graphics::Effects::IGraphicsEffectD2D1Interop;
using ABI::Windows::Graphics::Effects::IGraphicsEffectSource;

bool fail(std::string& error, const char* what, HRESULT hr) {
    error = std::format("{} failed: 0x{:08X}", what, static_cast<uint32_t>(hr));
    return false;
}

Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IPropertyValueStatics>
property_value_statics(std::string& error) {
    static Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> statics;
    if (statics) {
        return statics;
    }
    Microsoft::WRL::Wrappers::HStringReference class_name(
        RuntimeClass_Windows_Foundation_PropertyValue);
    HRESULT hr = RoGetActivationFactory(
        class_name.Get(),
        __uuidof(ABI::Windows::Foundation::IPropertyValueStatics),
        reinterpret_cast<void**>(statics.GetAddressOf()));
    if (FAILED(hr)) {
        fail(error, "RoGetActivationFactory(PropertyValue)", hr);
        return nullptr;
    }
    return statics;
}

class gaussian_blur_effect
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IInspectable,
          IGraphicsEffect,
          IGraphicsEffectSource,
          IGraphicsEffectD2D1Interop> {
public:
    gaussian_blur_effect(Microsoft::WRL::ComPtr<IGraphicsEffectSource> source,
                       float standard_deviation)
        : source_(std::move(source)), standard_deviation_(standard_deviation) {}

    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** iids) override {
        if (count == nullptr || iids == nullptr) {
            return E_POINTER;
        }
        *count = 0;
        *iids = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override {
        if (name == nullptr) {
            return E_POINTER;
        }
        *name = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override {
        if (level == nullptr) {
            return E_POINTER;
        }
        *level = BaseTrust;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Name(HSTRING* name) override {
        if (name == nullptr) {
            return E_POINTER;
        }
        return WindowsCreateString(name_.c_str(), static_cast<UINT32>(name_.size()), name);
    }

    HRESULT STDMETHODCALLTYPE put_Name(HSTRING name) override {
        if (name == nullptr) {
            name_.clear();
            return S_OK;
        }
        UINT32 length = 0;
        const wchar_t* text = WindowsGetStringRawBuffer(name, &length);
        name_.assign(text, length);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) override {
        if (id == nullptr) {
            return E_POINTER;
        }
        *id = kgaussian_blur_effect;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(
        LPCWSTR name, UINT* index,
        ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) override {
        if (name == nullptr || index == nullptr || mapping == nullptr) {
            return E_POINTER;
        }
        const wchar_t* dot = std::wcsrchr(name, L'.');
        const wchar_t* property = dot != nullptr ? dot + 1 : name;
        if (std::wcscmp(property, L"StandardDeviation") == 0) {
            *index = D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION;
            *mapping = GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
            return S_OK;
        }
        if (std::wcscmp(property, L"Optimization") == 0) {
            *index = D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION;
            *mapping = GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
            return S_OK;
        }
        if (std::wcscmp(property, L"BorderMode") == 0) {
            *index = D2D1_GAUSSIANBLUR_PROP_BORDER_MODE;
            *mapping = GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) override {
        if (count == nullptr) {
            return E_POINTER;
        }
        *count = 3;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetProperty(
        UINT index, ABI::Windows::Foundation::IPropertyValue** value) override {
        if (value == nullptr) {
            return E_POINTER;
        }
        std::string error;
        auto statics = property_value_statics(error);
        if (!statics) {
            return E_FAIL;
        }
        Microsoft::WRL::ComPtr<IInspectable> property;
        HRESULT hr = S_OK;
        if (index == D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION) {
            hr = statics->CreateSingle(standard_deviation_, property.GetAddressOf());
        } else if (index == D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION) {
            hr = statics->CreateUInt32(
                D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED, property.GetAddressOf());
        } else if (index == D2D1_GAUSSIANBLUR_PROP_BORDER_MODE) {
            hr = statics->CreateUInt32(D2D1_BORDER_MODE_SOFT, property.GetAddressOf());
        } else {
            return E_BOUNDS;
        }
        if (FAILED(hr)) {
            return hr;
        }
        Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IPropertyValue> converted;
        hr = property.As(&converted);
        if (FAILED(hr)) {
            return hr;
        }
        *value = converted.Detach();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSource(
        UINT index, IGraphicsEffectSource** source) override {
        if (source == nullptr) {
            return E_POINTER;
        }
        if (index != 0 || !source_) {
            return E_BOUNDS;
        }
        return source_.CopyTo(source);
    }

    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) override {
        if (count == nullptr) {
            return E_POINTER;
        }
        *count = 1;
        return S_OK;
    }

private:
    Microsoft::WRL::ComPtr<IGraphicsEffectSource> source_;
    std::wstring name_ = L"GaussianBlur";
    float standard_deviation_ = 0.0f;
};

}

bool create_blur_brush(
    ABI::Windows::UI::Composition::ICompositor* compositor,
    ABI::Windows::UI::Composition::ICompositionBrush* source,
    float standard_deviation,
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBrush>& result,
    std::string& error) {
    if (compositor == nullptr || source == nullptr) {
        error = "invalid effect arguments";
        return false;
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionEffectSourceParameterFactory>
        source_factory;
    Microsoft::WRL::Wrappers::HStringReference source_class(
        RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter);
    HRESULT hr = RoGetActivationFactory(
        source_class.Get(),
        __uuidof(ABI::Windows::UI::Composition::ICompositionEffectSourceParameterFactory),
        reinterpret_cast<void**>(source_factory.GetAddressOf()));
    if (FAILED(hr)) {
        return fail(error, "RoGetActivationFactory(CompositionEffectSourceParameter)", hr);
    }

    Microsoft::WRL::Wrappers::HStringReference source_name(L"source");
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionEffectSourceParameter>
        source_parameter;
    hr = source_factory->Create(source_name.Get(), source_parameter.GetAddressOf());
    if (FAILED(hr)) {
        return fail(error, "CreateEffectSourceParameter", hr);
    }
    Microsoft::WRL::ComPtr<IGraphicsEffectSource> effect_source;
    hr = source_parameter.As(&effect_source);
    if (FAILED(hr)) {
        return fail(error, "QueryInterface(IGraphicsEffectSource)", hr);
    }

    Microsoft::WRL::ComPtr<gaussian_blur_effect> effect =
        Microsoft::WRL::Make<gaussian_blur_effect>(effect_source, standard_deviation);
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionEffectFactory> factory;
    hr = compositor->CreateEffectFactory(effect.Get(), factory.GetAddressOf());
    if (FAILED(hr)) {
        return fail(error, "CreateEffectFactory", hr);
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionEffectBrush> effect_brush;
    hr = factory->CreateBrush(effect_brush.GetAddressOf());
    if (FAILED(hr)) {
        return fail(error, "CompositionEffectFactory::CreateBrush", hr);
    }
    hr = effect_brush->SetSourceParameter(source_name.Get(), source);
    if (FAILED(hr)) {
        return fail(error, "CompositionEffectBrush::SetSourceParameter", hr);
    }
    hr = effect_brush.As(&result);
    if (FAILED(hr)) {
        return fail(error, "QueryInterface(ICompositionBrush)", hr);
    }
    return true;
}

}
