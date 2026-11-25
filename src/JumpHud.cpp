#include "JumpHud.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

JumpCueHud* JumpCueHud::create() {
    auto* ret = new JumpCueHud();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool JumpCueHud::init() {
    if (!CCNode::init()) {
        return false;
    }

    m_canvas = CCDrawNode::create();
    if (m_canvas) {
        this->addChild(m_canvas, 0);
    }

    m_title = CCLabelBMFont::create("SALTA", "bigFont.fnt");
    if (m_title) {
        m_title->setScale(0.5f);
        this->addChild(m_title, 1);
    }

    m_sub = CCLabelBMFont::create("", "chatFont.fnt");
    if (m_sub) {
        m_sub->setScale(0.6f);
        m_sub->setColor(ccc3(210, 210, 210));
        this->addChild(m_sub, 1);
    }

    this->setVisible(false);
    return true;
}

void JumpCueHud::layout(float winWidth, float winHeight, bool topPosition) {
    float const y = topPosition ? winHeight * 0.86f : winHeight * 0.14f;
    this->setPosition(ccp(winWidth * 0.5f, y));
}

void JumpCueHud::drawBar(jumpcue::CueResult const& cue, bool jumpNow, float halfWidth) {
    if (!m_canvas) {
        return;
    }
    m_canvas->clear();

    float const x0 = -halfWidth;
    float const x1 = halfWidth;

    // Track.
    m_canvas->drawSegment(
        ccp(x0, 0.f), ccp(x1, 0.f), 6.f, ccc4f(0.0f, 0.0f, 0.0f, 0.55f)
    );

    // Frames in which jumping clears the obstacle.
    float const winLeft = x0 + (x1 - x0) * std::clamp(cue.barLast, 0.f, 1.f);
    float const winRight = x0 + (x1 - x0) * std::clamp(cue.barFirst, 0.f, 1.f);
    cocos2d::ccColor4F const windowColor = jumpNow
        ? ccc4f(0.20f, 1.0f, 0.35f, 0.95f)
        : ccc4f(1.0f, 0.80f, 0.15f, 0.85f);

    if (winRight - winLeft > 1.5f) {
        m_canvas->drawSegment(
            ccp(winLeft, 0.f), ccp(winRight, 0.f), 6.f, windowColor
        );
    }

    // "Now" marker at the right end.
    m_canvas->drawSegment(
        ccp(x1, -10.f), ccp(x1, 10.f), 1.6f, ccc4f(1.f, 1.f, 1.f, 0.9f)
    );

    if (jumpNow) {
        // Soft glow around the marker so it reads at a glance.
        m_canvas->drawSegment(
            ccp(x1 - 14.f, 0.f), ccp(x1, 0.f), 6.f, ccc4f(0.2f, 1.f, 0.35f, 0.35f)
        );
    }
}

void JumpCueHud::refresh(jumpcue::CueResult const& cue, Style const& style) {
    bool const show = style.enabled && cue.found && cue.firstDelay >= 0;
    this->setVisible(show);
    if (!show) {
        m_pulse = 0.f;
        return;
    }

    auto const winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
    this->layout(winSize.width, winSize.height, style.topPosition);

    float const halfWidth = std::max(90.f, winSize.width * 0.16f);
    bool const jumpNow = cue.jumpNow;

    this->drawBar(cue, jumpNow, halfWidth);

    if (m_title) {
        if (jumpNow) {
            m_title->setString(style.spanish ? "SALTA!" : "JUMP!");
            m_title->setColor(ccc3(70, 255, 110));
            m_pulse += 0.35f;
            float const scale = 0.52f + 0.05f * std::sin(m_pulse);
            m_title->setScale(scale);
        } else {
            m_title->setString(
                fmt::format(style.spanish ? "en {:.2f}s" : "in {:.2f}s", cue.secondsToWindow)
                    .c_str()
            );
            m_title->setColor(ccc3(255, 214, 60));
            m_title->setScale(0.42f);
            m_pulse = 0.f;
        }
        m_title->setPosition(ccp(0.f, 22.f));
    }

    if (m_sub) {
        if (style.showSubLabel) {
            m_sub->setString(
                fmt::format(
                    style.spanish ? "impacto en {:.2f}s" : "impact in {:.2f}s",
                    cue.secondsToImpact
                ).c_str()
            );
            m_sub->setVisible(true);
            m_sub->setPosition(ccp(0.f, -18.f));
        } else {
            m_sub->setVisible(false);
        }
    }
}
