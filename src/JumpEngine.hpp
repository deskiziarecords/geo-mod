#pragma once

// Pure-logic "when should I press jump" engine.
//
// This file intentionally has no Geometry Dash / Geode dependencies so the
// timing maths can be reasoned about (and unit tested) on its own.
//
// Coordinate system: GD object units, 1 block == 30 units, +x right, +y up.
// Time is measured in 60 Hz physics steps ("frames"); one step == 1/60 s.
//
// The engine answers one question for the nearest hazard in front of the
// player: "for which of the next N frames would pressing jump now / pressing
// jump in N frames clear that hazard?"  That set of frames is the jump window.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace jumpcue {

// One physics step of Geometry Dash's 60 Hz simulation.
inline constexpr float kStepSeconds = 1.0f / 60.0f;

struct Rect {
    float left = 0.f;
    float bottom = 0.f;
    float right = 0.f;
    float top = 0.f;

    bool empty() const {
        return right <= left || top <= bottom;
    }

    bool intersects(Rect const& other) const {
        if (empty() || other.empty()) {
            return false;
        }
        return !(right <= other.left || other.right <= left ||
                 top <= other.bottom || other.top <= bottom);
    }

    float width() const { return right - left; }
    float height() const { return top - bottom; }

    static Rect fromCenter(float cx, float cy, float halfW, float halfH) {
        return Rect{cx - halfW, cy - halfH, cx + halfW, cy + halfH};
    }
};

// Vertical offset (relative to the position the jump started from) for every
// 60 Hz step of a jump, starting at 0. Sampled from the player's own jumps so
// the engine does not have to hardcode GD's gravity / jump impulse (which
// differ between game modes and speed portals).
struct ArcProfile {
    std::vector<float> offsets;

    bool valid() const { return offsets.size() >= 6; }
    int frames() const { return static_cast<int>(offsets.size()); }

    float at(int frame) const {
        if (offsets.empty()) {
            return 0.f;
        }
        if (frame < 0) {
            return offsets.front();
        }
        if (frame >= static_cast<int>(offsets.size())) {
            return offsets.back();
        }
        return offsets[static_cast<std::size_t>(frame)];
    }

    float peak() const {
        float best = 0.f;
        for (float v : offsets) {
            best = std::max(best, v);
        }
        return best;
    }

    int peakFrame() const {
        int best = 0;
        float bestValue = -1.f;
        for (int i = 0; i < frames(); ++i) {
            if (offsets[static_cast<std::size_t>(i)] > bestValue) {
                bestValue = offsets[static_cast<std::size_t>(i)];
                best = i;
            }
        }
        return best;
    }

    // Generic cube-ish jump used until the first real jump is observed.
    // Peak ~2 blocks, airtime ~26 steps (values for 1x speed, all speeds share
    // the same *vertical* arc in GD because gravity does not scale with speed).
    static ArcProfile fallback();
};

struct PlayerState {
    float x = 0.f;
    float y = 0.f;
    // Horizontal speed in units per physics step (always positive in GD).
    float vx = 0.f;
    float halfW = 12.75f;
    float halfH = 12.75f;
    bool grounded = true;
    ArcProfile arc;
    // How far ahead we are willing to look, in steps.
    int maxDelayFrames = 60;
};

struct CueResult {
    bool found = false;      // a hazard worth cueing was found
    bool jumpNow = false;    // pressing jump on this very frame clears it
    int firstDelay = -1;     // first frame (0 = now) that clears the hazard
    int lastDelay = -1;      // last frame that still clears it
    int impactDelay = -1;    // frames until we would hit it without jumping
    float secondsToWindow = 0.f;
    float secondsToImpact = 0.f;
    // Normalised bar positions for the UI: 0 = right end (now),
    // 1 = left end (maxDelayFrames in the future).
    float barFirst = 0.f;
    float barLast = 0.f;
};

// Nearest hazard in front of the player, or nullptr.
Rect const* nearestHazard(PlayerState const& state, std::vector<Rect> const& hazards);

// Solve the jump window for the nearest hazard.
CueResult solve(PlayerState const& state, std::vector<Rect> const& hazards);

// Learns the vertical jump arc from the player's own jumps.
class ArcCalibrator {
public:
    void reset();

    // Call once per frame with the player's y position and grounded state.
    // Returns true on the frame a complete new arc has been captured.
    bool update(float y, bool grounded, float dt);

    ArcProfile const& profile() const { return m_profile; }
    float confidence() const { return m_confidence; }

private:
    void finishCapture();

    ArcProfile m_profile = ArcProfile::fallback();
    float m_confidence = 0.f;
    bool m_recording = false;
    bool m_wasGrounded = true;
    float m_startY = 0.f;
    float m_accumulator = 0.f;
    std::vector<float> m_capture;
    int m_captures = 0;
};

} // namespace jumpcue
