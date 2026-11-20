#include "gfx/wuc_animator.h"

#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include <algorithm>
#include <format>

namespace flux::gfx {

namespace {

using BatchHandler = ABI::Windows::Foundation::ITypedEventHandler<
    IInspectable*, ABI::Windows::UI::Composition::CompositionBatchCompletedEventArgs*>;

bool fail(std::string& error, const char* what, HRESULT hr) {
    error = std::format("{} failed: 0x{:08X}", what, static_cast<uint32_t>(hr));
    return false;
}

ABI::Windows::Foundation::TimeSpan time_span(float seconds) {
    ABI::Windows::Foundation::TimeSpan span = {};
    span.Duration = static_cast<LONGLONG>(std::max(0.0f, seconds) * 10000000.0f);
    return span;
}

const wchar_t* property_name(flux::gfx::layer_property property) {
    switch (property) {
    case flux::gfx::layer_property::opacity: return L"Opacity";
    case flux::gfx::layer_property::offset: return L"Offset";
    case flux::gfx::layer_property::scale: return L"Scale";
    case flux::gfx::layer_property::rotation: return L"RotationAngleInDegrees";
    default: return L"Opacity";
    }
}

bool is_vector(flux::gfx::layer_property property) {
    return property == flux::gfx::layer_property::offset || property == flux::gfx::layer_property::scale;
}

ABI::Windows::Foundation::Numerics::Vector3 to_vector3(flux::gfx::layer_property property,
                                                       const flux::gfx::layer_value& value,
                                                       float dpi_scale) {
    ABI::Windows::Foundation::Numerics::Vector3 result = {};
    if (property == flux::gfx::layer_property::offset) {
        result.X = value.x * dpi_scale;
        result.Y = value.y * dpi_scale;
        result.Z = 0.0f;
    } else if (property == flux::gfx::layer_property::scale) {
        result.X = value.x;
        result.Y = value.y;
        result.Z = 1.0f;
    } else {
        result.X = value.x;
        result.Y = value.y;
        result.Z = value.z;
    }
    return result;
}

float to_scalar(const flux::gfx::layer_value& value) {
    return value.x;
}

template <typename T>
class reference_value : public Microsoft::WRL::RuntimeClass<
                           Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
                           ABI::Windows::Foundation::IReference<T>> {
public:
    explicit reference_value(const T& value) : value_(value) {}

    HRESULT STDMETHODCALLTYPE get_Value(T* value) override {
        if (value == nullptr) {
            return E_POINTER;
        }
        *value = value_;
        return S_OK;
    }

private:
    T value_;
};

}

class wuc_animator::completed_handler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          BatchHandler> {
public:
    completed_handler(HWND hwnd, uint64_t id) : hwnd_(hwnd), id_(id) {}

    HRESULT STDMETHODCALLTYPE Invoke(
        IInspectable*,
        ABI::Windows::UI::Composition::ICompositionBatchCompletedEventArgs*) override {
        PostMessageW(
            hwnd_,
            k_animation_completed_message,
            static_cast<WPARAM>(static_cast<uint32_t>(id_ & 0xFFFFFFFFull)),
            static_cast<LPARAM>(static_cast<uint32_t>(id_ >> 32)));
        return S_OK;
    }

private:
    HWND hwnd_ = nullptr;
    uint64_t id_ = 0;
};

bool wuc_animator::init(HWND hwnd, ABI::Windows::UI::Composition::ICompositor* compositor,
                       std::string& error) {
    shutdown();
    if (hwnd == nullptr || compositor == nullptr) {
        error = "invalid animator arguments";
        return false;
    }
    hwnd_ = hwnd;
    compositor_ = compositor;
    compositor4_.Reset();
    compositor_.As(&compositor4_);
    return true;
}

void wuc_animator::shutdown() {
    for (auto& entry : records_) {
        record& record = entry.second;
        if (record.has_token && record.batch) {
            record.batch->remove_Completed(record.token);
        }
    }
    records_.clear();
    compositor4_.Reset();
    compositor_.Reset();
    hwnd_ = nullptr;
    next_id_ = 1;
}

uint64_t wuc_animator::start(ABI::Windows::UI::Composition::IVisual* visual,
                            const flux::gfx::layer_animation_spec& spec, float dpi_scale, uint64_t id,
                            std::string& error) {
    if (compositor_ == nullptr || visual == nullptr || hwnd_ == nullptr) {
        error = "animator is not initialized";
        return 0;
    }
    if (spec.tracks.empty()) {
        error = "animation has no tracks";
        return 0;
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionObject> object;
    HRESULT hr = visual->QueryInterface(
        __uuidof(ABI::Windows::UI::Composition::ICompositionObject),
        reinterpret_cast<void**>(object.GetAddressOf()));
    if (FAILED(hr)) {
        fail(error, "QueryInterface(ICompositionObject)", hr);
        return 0;
    }

    bool infinite = false;
    for (const flux::gfx::layer_animation_track& track : spec.tracks) {
        if (track.iterations <= 0) {
            infinite = true;
            break;
        }
        if (track.keyframes.empty()) {
            error = "animation track has no keyframes";
            return 0;
        }
    }

    record record;
    record.object = object;
    record.infinite = infinite;

    const uint64_t animation_id = id != 0 ? id : next_id_++;
    std::vector<Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionAnimation>>
        animations;
    animations.reserve(spec.tracks.size());

    for (const flux::gfx::layer_animation_track& track : spec.tracks) {
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionAnimation> animation;
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionEasingFunction> easing;

        const bool vector = is_vector(track.property);
        if (!track.linear) {
            ABI::Windows::Foundation::Numerics::Vector2 point1 = {};
            point1.X = track.easing_x1;
            point1.Y = track.easing_y1;
            ABI::Windows::Foundation::Numerics::Vector2 point2 = {};
            point2.X = track.easing_x2;
            point2.Y = track.easing_y2;
            Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICubicBezierEasingFunction> cubic;
            hr = compositor_->CreateCubicBezierEasingFunction(
                point1, point2, cubic.GetAddressOf());
            if (FAILED(hr)) {
                fail(error, "CreateCubicBezierEasingFunction", hr);
                return 0;
            }
            hr = cubic.As(&easing);
            if (FAILED(hr)) {
                fail(error, "QueryInterface(ICompositionEasingFunction)", hr);
                return 0;
            }
        }

        if (track.spring) {
            if (compositor4_ == nullptr) {
                error = "spring animations require ICompositor4";
                return 0;
            }
            if (vector) {
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ISpringVector3NaturalMotionAnimation>
                    spring;
                hr = compositor4_->CreateSpringVector3Animation(spring.GetAddressOf());
                if (FAILED(hr)) {
                    fail(error, "CreateSpringVector3Animation", hr);
                    return 0;
                }
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ISpringVector3NaturalMotionAnimation>
                    spring_params = spring;
                spring_params->put_DampingRatio(track.damping_ratio);
                spring_params->put_Period(time_span(track.period));

                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IVector3NaturalMotionAnimation>
                    motion;
                hr = spring.As(&motion);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(IVector3NaturalMotionAnimation)", hr);
                    return 0;
                }
                const auto initial = to_vector3(track.property, track.keyframes.front().value,
                                                dpi_scale);
                const auto final = to_vector3(track.property, track.keyframes.back().value,
                                              dpi_scale);
                Microsoft::WRL::ComPtr<reference_value<ABI::Windows::Foundation::Numerics::Vector3>>
                    initial_ref = Microsoft::WRL::Make<
                        reference_value<ABI::Windows::Foundation::Numerics::Vector3>>(initial);
                Microsoft::WRL::ComPtr<reference_value<ABI::Windows::Foundation::Numerics::Vector3>>
                    final_ref = Microsoft::WRL::Make<
                        reference_value<ABI::Windows::Foundation::Numerics::Vector3>>(final);
                motion->put_InitialValue(initial_ref.Get());
                motion->put_FinalValue(final_ref.Get());

                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::INaturalMotionAnimation> natural;
                hr = spring.As(&natural);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(INaturalMotionAnimation)", hr);
                    return 0;
                }
                natural->put_DelayTime(time_span(track.delay));
                natural->put_StopBehavior(
                    ABI::Windows::UI::Composition::AnimationStopBehavior_SetToFinalValue);
                hr = spring.As(&animation);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(ICompositionAnimation)", hr);
                    return 0;
                }
            } else {
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ISpringScalarNaturalMotionAnimation>
                    spring;
                hr = compositor4_->CreateSpringScalarAnimation(spring.GetAddressOf());
                if (FAILED(hr)) {
                    fail(error, "CreateSpringScalarAnimation", hr);
                    return 0;
                }
                spring->put_DampingRatio(track.damping_ratio);
                spring->put_Period(time_span(track.period));

                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IScalarNaturalMotionAnimation>
                    motion;
                hr = spring.As(&motion);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(IScalarNaturalMotionAnimation)", hr);
                    return 0;
                }
                Microsoft::WRL::ComPtr<reference_value<float>> initial_ref =
                    Microsoft::WRL::Make<reference_value<float>>(
                        to_scalar(track.keyframes.front().value));
                Microsoft::WRL::ComPtr<reference_value<float>> final_ref =
                    Microsoft::WRL::Make<reference_value<float>>(
                        to_scalar(track.keyframes.back().value));
                motion->put_InitialValue(initial_ref.Get());
                motion->put_FinalValue(final_ref.Get());
                motion->put_InitialVelocity(track.initial_velocity);

                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::INaturalMotionAnimation> natural;
                hr = spring.As(&natural);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(INaturalMotionAnimation)", hr);
                    return 0;
                }
                natural->put_DelayTime(time_span(track.delay));
                natural->put_StopBehavior(
                    ABI::Windows::UI::Composition::AnimationStopBehavior_SetToFinalValue);
                hr = spring.As(&animation);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(ICompositionAnimation)", hr);
                    return 0;
                }
            }
        } else {
            if (track.keyframes.size() < 2) {
                error = "keyframe animation needs at least two keyframes";
                return 0;
            }
            if (vector) {
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IVector3KeyFrameAnimation>
                    keyframe;
                hr = compositor_->CreateVector3KeyFrameAnimation(keyframe.GetAddressOf());
                if (FAILED(hr)) {
                    fail(error, "CreateVector3KeyFrameAnimation", hr);
                    return 0;
                }
                for (size_t i = 0; i < track.keyframes.size(); ++i) {
                    const flux::gfx::layer_keyframe& key = track.keyframes[i];
                    const auto value = to_vector3(track.property, key.value, dpi_scale);
                    if (!track.linear && i + 1 == track.keyframes.size() && easing) {
                        keyframe->InsertKeyFrameWithEasingFunction(key.t, value, easing.Get());
                    } else {
                        keyframe->InsertKeyFrame(key.t, value);
                    }
                }
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IKeyFrameAnimation> timing;
                hr = keyframe.As(&timing);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(IKeyFrameAnimation)", hr);
                    return 0;
                }
                timing->put_DelayTime(time_span(track.delay));
                timing->put_Duration(time_span(track.duration));
                timing->put_StopBehavior(
                    ABI::Windows::UI::Composition::AnimationStopBehavior_SetToFinalValue);
                if (track.iterations <= 0) {
                    timing->put_IterationBehavior(
                        ABI::Windows::UI::Composition::AnimationIterationBehavior_Forever);
                } else {
                    timing->put_IterationBehavior(
                        ABI::Windows::UI::Composition::AnimationIterationBehavior_Count);
                    timing->put_IterationCount(track.iterations);
                }
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IKeyFrameAnimation2> direction;
                if (SUCCEEDED(keyframe.As(&direction))) {
                    direction->put_Direction(
                        static_cast<ABI::Windows::UI::Composition::AnimationDirection>(
                            track.direction));
                }
                hr = keyframe.As(&animation);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(ICompositionAnimation)", hr);
                    return 0;
                }
            } else {
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IScalarKeyFrameAnimation>
                    keyframe;
                hr = compositor_->CreateScalarKeyFrameAnimation(keyframe.GetAddressOf());
                if (FAILED(hr)) {
                    fail(error, "CreateScalarKeyFrameAnimation", hr);
                    return 0;
                }
                for (size_t i = 0; i < track.keyframes.size(); ++i) {
                    const flux::gfx::layer_keyframe& key = track.keyframes[i];
                    const float value = to_scalar(key.value);
                    if (!track.linear && i + 1 == track.keyframes.size() && easing) {
                        keyframe->InsertKeyFrameWithEasingFunction(key.t, value, easing.Get());
                    } else {
                        keyframe->InsertKeyFrame(key.t, value);
                    }
                }
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IKeyFrameAnimation> timing;
                hr = keyframe.As(&timing);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(IKeyFrameAnimation)", hr);
                    return 0;
                }
                timing->put_DelayTime(time_span(track.delay));
                timing->put_Duration(time_span(track.duration));
                timing->put_StopBehavior(
                    ABI::Windows::UI::Composition::AnimationStopBehavior_SetToFinalValue);
                if (track.iterations <= 0) {
                    timing->put_IterationBehavior(
                        ABI::Windows::UI::Composition::AnimationIterationBehavior_Forever);
                } else {
                    timing->put_IterationBehavior(
                        ABI::Windows::UI::Composition::AnimationIterationBehavior_Count);
                    timing->put_IterationCount(track.iterations);
                }
                Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IKeyFrameAnimation2> direction;
                if (SUCCEEDED(keyframe.As(&direction))) {
                    direction->put_Direction(
                        static_cast<ABI::Windows::UI::Composition::AnimationDirection>(
                            track.direction));
                }
                hr = keyframe.As(&animation);
                if (FAILED(hr)) {
                    fail(error, "QueryInterface(ICompositionAnimation)", hr);
                    return 0;
                }
            }
        }

        animations.push_back(animation);
    }

    if (!infinite) {
        hr = compositor_->CreateScopedBatch(
            ABI::Windows::UI::Composition::CompositionBatchTypes_Animation,
            record.batch.GetAddressOf());
        if (FAILED(hr)) {
            fail(error, "CreateScopedBatch", hr);
            return 0;
        }
        Microsoft::WRL::ComPtr<completed_handler> completed =
            Microsoft::WRL::Make<completed_handler>(hwnd_, animation_id);
        hr = completed.As(&record.handler);
        if (FAILED(hr)) {
            fail(error, "QueryInterface(ITypedEventHandler)", hr);
            return 0;
        }
        hr = record.batch->add_Completed(record.handler.Get(), &record.token);
        if (FAILED(hr)) {
            fail(error, "ICompositionScopedBatch::add_Completed", hr);
            return 0;
        }
        record.has_token = true;
    }

    for (size_t i = 0; i < spec.tracks.size(); ++i) {
        Microsoft::WRL::Wrappers::HStringReference name(property_name(spec.tracks[i].property));
        hr = object->StartAnimation(name.Get(), animations[i].Get());
        if (FAILED(hr)) {
            fail(error, "ICompositionObject::StartAnimation", hr);
            if (record.has_token) {
                record.batch->remove_Completed(record.token);
            }
            for (size_t j = 0; j < spec.tracks.size(); ++j) {
                Microsoft::WRL::Wrappers::HStringReference stop_name(
                    property_name(spec.tracks[j].property));
                object->StopAnimation(stop_name.Get());
            }
            return 0;
        }
        record.properties.push_back(property_name(spec.tracks[i].property));
    }

    if (record.batch) {
        hr = record.batch->End();
        if (FAILED(hr)) {
            fail(error, "ICompositionScopedBatch::End", hr);
            for (const std::wstring& property : record.properties) {
                Microsoft::WRL::Wrappers::HStringReference name(property.c_str());
                object->StopAnimation(name.Get());
            }
            if (record.has_token) {
                record.batch->remove_Completed(record.token);
            }
            return 0;
        }
    }

    record.animations = std::move(animations);
    records_[animation_id] = std::move(record);
    return animation_id;
}

bool wuc_animator::stop(uint64_t id) {
    auto found = records_.find(id);
    if (found == records_.end()) {
        return false;
    }
    record& record = found->second;
    if (record.has_token && record.batch) {
        record.batch->remove_Completed(record.token);
    }
    if (record.object) {
        for (const std::wstring& property : record.properties) {
            Microsoft::WRL::Wrappers::HStringReference name(property.c_str());
            record.object->StopAnimation(name.Get());
        }
    }
    records_.erase(found);
    return true;
}

bool wuc_animator::finish(uint64_t id) {
    auto found = records_.find(id);
    if (found == records_.end()) {
        return false;
    }
    record& record = found->second;
    if (record.has_token && record.batch) {
        record.batch->remove_Completed(record.token);
    }
    records_.erase(found);
    return true;
}

bool wuc_animator::running(uint64_t id) const {
    return records_.find(id) != records_.end();
}

int wuc_animator::active_count() const {
    return static_cast<int>(records_.size());
}

std::vector<uint64_t> wuc_animator::cancel_all() {
    std::vector<uint64_t> ids;
    ids.reserve(records_.size());
    for (const auto& entry : records_) {
        ids.push_back(entry.first);
    }
    for (uint64_t id : ids) {
        stop(id);
    }
    return ids;
}

bool wuc_animator::system_animations_enabled() {
    BOOL enabled = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        return true;
    }
    return enabled != FALSE;
}

}
