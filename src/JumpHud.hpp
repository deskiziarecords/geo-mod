#pragma once

#include <Geode/Geode.hpp>

#include "JumpEngine.hpp"

// Small screen-space overlay: a timing bar plus a label.
//
// The bar's right end is "now" and its left end is the end of the look-ahead
// window. The green part of the bar is the span of frames in which pressing
// jump still clears the upcoming obstacle, so the cue is to press while the
// bar is lit / while the text says SALTA.
class JumpCueHud : public cocos2d::CCNode {
public:
    static JumpCueHud* create();

    struct Style {
        bool enabled = true;
        bool spanish = true;
        bool showSubLabel = true;
        bool topPosition = false;
    };

    void refresh(jumpcue::CueResult const& cue, Style const& style);

    bool init() override;

private:
    void layout(float winWidth, float winHeight, bool topPosition);
    void drawBar(jumpcue::CueResult const& cue, bool jumpNow, float halfWidth);

    cocos2d::CCDrawNode* m_canvas = nullptr;
    cocos2d::CCLabelBMFont* m_title = nullptr;
    cocos2d::CCLabelBMFont* m_sub = nullptr;
    float m_pulse = 0.f;
};
