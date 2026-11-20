#pragma once

#include <windows.h>
#include <windows.foundation.h>
#include <windows.graphics.effects.h>
#include <windows.graphics.effects.interop.h>
#include <windows.ui.composition.h>
#include <wrl/client.h>

#include <string>

namespace flux::gfx {

bool create_blur_brush(
    ABI::Windows::UI::Composition::ICompositor* compositor,
    ABI::Windows::UI::Composition::ICompositionBrush* source,
    float standard_deviation,
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBrush>& result,
    std::string& error);

}
