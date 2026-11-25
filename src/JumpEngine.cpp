#include "JumpEngine.hpp"

namespace jumpcue {
namespace {

float sampleLinear(std::vector<float> const& values, float t) {
    if (values.empty()) {
        return 0.f;
    }
    if (values.size() == 1) {
        return values.front();
    }
    float const clamped = std::clamp(t, 0.f, 1.f);
    float const x = clamped * static_cast<float>(values.size() - 1);
    auto const i0 = static_cast<std::size_t>(std::floor(x));
    auto const i1 = std::min(values.size() - 1, i0 + 1);
    float const frac = x - static_cast<float>(i0);
    return values[i0] * (1.f - frac) + values[i1] * frac;
}

} // namespace

ArcProfile ArcProfile::fallback() {
    ArcProfile profile;
    constexpr int kSteps = 26;
    constexpr float kPeak = 62.f;
    profile.offsets.reserve(static_cast<std::size_t>(kSteps) + 1);
    for (int i = 0; i <= kSteps; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(kSteps);
        profile.offsets.push_back(4.f * kPeak * t * (1.f - t));
    }
    return profile;
}

Rect const* nearestHazard(PlayerState const& state, std::vector<Rect> const& hazards) {
    Rect const* best = nullptr;
    float bestLeft = 1e30f;
    for (auto const& hazard : hazards) {
        if (hazard.empty()) {
            continue;
        }
        // Already behind (or currently overlapping) the player.
        if (hazard.right <= state.x - state.halfW) {
            continue;
        }
        // Only hazards that would actually hit us while running on the ground
        // are worth jumping over: spikes at head height, hazards on a lower
        // platform, etc. must not trigger a cue.
        bool const inTheWay =
            hazard.bottom < state.y + state.halfH && hazard.top > state.y - state.halfH;
        if (!inTheWay) {
            continue;
        }
        if (hazard.left < bestLeft) {
            bestLeft = hazard.left;
            best = &hazard;
        }
    }
    return best;
}

CueResult solve(PlayerState const& state, std::vector<Rect> const& hazards) {
    CueResult result;
    if (state.vx <= 0.01f || !state.arc.valid()) {
        return result;
    }

    Rect const* target = nearestHazard(state, hazards);
    if (!target) {
        return result;
    }
    Rect const hazard = *target;
    result.found = true;

    float const gap = hazard.left - (state.x + state.halfW);
    int const impact = gap <= 0.f ? 0
                                  : static_cast<int>(std::ceil(gap / state.vx));
    result.impactDelay = impact;
    result.secondsToImpact = static_cast<float>(impact) * kStepSeconds;

    int const maxDelay = std::max(0, std::min(state.maxDelayFrames, impact));
    int const arcFrames = state.arc.frames();

    int first = -1;
    int last = -1;

    for (int delay = 0; delay <= maxDelay; ++delay) {
        bool ok = true;

        // Frames spent waiting on the ground before pressing jump.
        for (int f = 0; f < delay && ok; ++f) {
            Rect const body = Rect::fromCenter(
                state.x + state.vx * static_cast<float>(f), state.y, state.halfW, state.halfH
            );
            if (body.intersects(hazard)) {
                ok = false;
            }
        }

        // Airborne part of the jump.
        for (int f = 0; f < arcFrames && ok; ++f) {
            Rect const body = Rect::fromCenter(
                state.x + state.vx * static_cast<float>(delay + f),
                state.y + state.arc.at(f),
                state.halfW,
                state.halfH
            );
            if (body.intersects(hazard)) {
                ok = false;
            }
        }

        // Back on the ground after landing: if we land in front of the hazard
        // we would just run into it, so that jump does not clear it.
        for (int f = delay + arcFrames, guard = 0; ok && guard < 240; ++f, ++guard) {
            float const x = state.x + state.vx * static_cast<float>(f);
            if (x - state.halfW > hazard.right) {
                break;
            }
            Rect const body =
                Rect::fromCenter(x, state.y, state.halfW, state.halfH);
            if (body.intersects(hazard)) {
                ok = false;
            }
        }

        if (ok) {
            if (first < 0) {
                first = delay;
            }
            last = delay;
        }
    }

    if (first < 0) {
        // No frame in the look-ahead window clears this hazard (it is probably
        // already unreachable, or it is a hazard the cube cannot jump over).
        return result;
    }

    result.firstDelay = first;
    result.lastDelay = last;
    result.jumpNow = first == 0;
    result.secondsToWindow = static_cast<float>(first) * kStepSeconds;

    float const span = static_cast<float>(std::max(1, state.maxDelayFrames));
    result.barFirst = 1.f - std::min(1.f, static_cast<float>(first) / span);
    result.barLast = 1.f - std::min(1.f, static_cast<float>(last) / span);

    return result;
}

void ArcCalibrator::reset() {
    m_profile = ArcProfile::fallback();
    m_confidence = 0.f;
    m_recording = false;
    m_wasGrounded = true;
    m_startY = 0.f;
    m_accumulator = 0.f;
    m_capture.clear();
    m_captures = 0;
}

bool ArcCalibrator::update(float y, bool grounded, float dt) {
    bool completed = false;

    if (!m_recording) {
        if (m_wasGrounded && !grounded) {
            m_recording = true;
            m_startY = y;
            m_capture.clear();
            m_capture.push_back(0.f);
            m_accumulator = 0.f;
        }
    } else if (grounded) {
        if (m_capture.size() >= 8) {
            finishCapture();
            completed = true;
        }
        m_recording = false;
        m_capture.clear();
    } else {
        m_accumulator += std::max(0.f, dt);
        while (m_accumulator >= kStepSeconds) {
            m_accumulator -= kStepSeconds;
            m_capture.push_back(y - m_startY);
            if (m_capture.size() > 240) {
                // This was not a regular jump (gravity portal, vehicle, ...).
                m_recording = false;
                m_capture.clear();
                break;
            }
        }
    }

    m_wasGrounded = grounded;
    return completed;
}

void ArcCalibrator::finishCapture() {
    ArcProfile candidate;
    candidate.offsets = m_capture;
    float const peak = candidate.peak();

    // Reject bumps, slides and broken captures.
    if (candidate.frames() < 12 || peak < 20.f || peak > 400.f) {
        return;
    }
    if (candidate.offsets.back() > peak * 0.6f) {
        return;
    }

    bool const similar = m_confidence > 0.05f && m_profile.valid() &&
        std::fabs(m_profile.peak() - peak) < peak * 0.35f &&
        std::fabs(static_cast<float>(m_profile.frames() - candidate.frames()))
                <= static_cast<float>(candidate.frames()) * 0.4f;

    if (!similar) {
        m_profile = candidate;
        m_confidence = std::min(1.f, m_confidence + 0.5f);
        m_captures = 1;
        return;
    }

    // Smooth the observed arc into the current model so single odd jumps do
    // not ruin the prediction.
    int const length = std::max(8, std::min(m_profile.frames(), candidate.frames()));
    ArcProfile merged;
    merged.offsets.resize(static_cast<std::size_t>(length));
    constexpr float kWeight = 0.3f;
    for (int i = 0; i < length; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(length - 1);
        float const a = sampleLinear(m_profile.offsets, t);
        float const b = sampleLinear(candidate.offsets, t);
        merged.offsets[static_cast<std::size_t>(i)] = a * (1.f - kWeight) + b * kWeight;
    }
    merged.offsets.front() = 0.f;

    m_profile = merged;
    m_confidence = std::min(1.f, m_confidence + 0.25f);
    ++m_captures;
}

} // namespace jumpcue
