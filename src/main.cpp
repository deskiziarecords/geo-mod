#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "JumpEngine.hpp"
#include "JumpHud.hpp"

using namespace geode::prelude;

namespace {

constexpr float kPlayerHalfSize = 15.f; // 30x30 unit hitbox
constexpr int kHazardRescanFrames = 12;

bool isHazard(GameObject* obj) {
    if (!obj || !obj->getParent() || !obj->isVisible()) {
        return false;
    }
    auto const type = obj->getType();
    return type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard;
}

jumpcue::Rect toRect(cocos2d::CCRect const& rect) {
    return jumpcue::Rect{
        rect.origin.x,
        rect.origin.y,
        rect.origin.x + rect.size.width,
        rect.origin.y + rect.size.height,
    };
}

} // namespace

class $modify(JumpCuePlayLayer, PlayLayer) {
    struct Fields {
        jumpcue::ArcCalibrator calibrator;
        jumpcue::PlayerState state;
        JumpCueHud* hud = nullptr;

        std::vector<GameObject*> hazards;
        PlayerObject* followed = nullptr;

        float lastX = 0.f;
        float lastY = 0.f;
        float speed = 0.f; // units per second, smoothed
        int scanTimer = 0;
        int frameCounter = 0;

        bool wasJumpNow = false;
        float soundCooldown = 0.f;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->tickCue(dt);
    }

    void resetTracking(PlayerObject* player, cocos2d::CCPoint const& pos) {
        auto* f = m_fields.self();
        f->followed = player;
        f->lastX = pos.x;
        f->lastY = pos.y;
        f->speed = 0.f;
        f->scanTimer = 0;
        f->frameCounter = 0;
        f->wasJumpNow = false;
        f->soundCooldown = 0.f;
        f->hazards.clear();
        f->calibrator.reset();
    }

    bool looksGrounded(PlayerObject* player, float dy) const {
        // GD exposes the authoritative flag; the fallback keeps the mod usable
        // if it ever changes (flat ground: y is perfectly still, vy is zero).
        if (player->m_isOnGround) {
            return true;
        }
        float const vy = static_cast<float>(player->getYVelocity());
        return std::fabs(dy) < 0.5f && std::fabs(vy) < 1.5f;
    }

    void rebuildHazards() {
        auto* f = m_fields.self();
        f->hazards.clear();

        auto* objects = this->m_objects;
        if (!objects) {
            return;
        }
        f->hazards.reserve(64);
        unsigned int const count = objects->count();
        for (unsigned int i = 0; i < count; ++i) {
            auto* object = static_cast<GameObject*>(objects->objectAtIndex(i));
            if (isHazard(object)) {
                f->hazards.push_back(object);
            }
        }
    }

    void ensureHud() {
        auto* f = m_fields.self();
        if (f->hud) {
            return;
        }
        f->hud = JumpCueHud::create();
        if (f->hud) {
            this->addChild(f->hud, 100);
        }
    }

    void playCueSound(float volume) {
        auto const path = (Mod::get()->getResourcesDir() / "jump_cue.wav").string();
        // (path, speed, unknown, volume) -- keep the two middle ones neutral.
        FMODAudioEngine::sharedEngine()->playEffect(path, 1.f, 1.f, volume);
    }

    void tickCue(float dt) {
        auto* f = m_fields.self();
        ensureHud();

        auto* mod = Mod::get();
        bool const enabled = mod->getSettingValue<bool>("enabled");
        if (!enabled) {
            if (f->hud) {
                f->hud->setVisible(false);
            }
            return;
        }

        auto* player = this->m_player1;
        if (!player) {
            return;
        }

        auto const pos = player->getPosition();
        if (f->followed != player) {
            resetTracking(player, pos);
            return;
        }
        if (dt <= 0.f) {
            f->lastX = pos.x;
            f->lastY = pos.y;
            return;
        }

        float const dx = pos.x - f->lastX;
        float const dy = pos.y - f->lastY;

        // Level restart, start-pos teleport or death respawn.
        if (dx < -80.f || dx > 600.f || std::fabs(dy) > 800.f) {
            resetTracking(player, pos);
            return;
        }

        float const measuredSpeed = dx / dt;
        if (measuredSpeed > 0.5f && measuredSpeed < 4000.f) {
            f->speed = f->speed <= 0.f ? measuredSpeed
                                       : f->speed + (measuredSpeed - f->speed) * 0.25f;
        }

        bool const grounded = looksGrounded(player, dy);

        // Learn the vertical arc from the player's own jumps.
        f->calibrator.update(pos.y, grounded, dt);

        f->lastX = pos.x;
        f->lastY = pos.y;

        if (f->speed <= 0.5f || !grounded) {
            if (f->hud) {
                f->hud->setVisible(false);
            }
            f->wasJumpNow = false;
            return;
        }

        // ---- look for hazards -------------------------------------------------
        if (--f->scanTimer <= 0 || f->hazards.empty()) {
            rebuildHazards();
            f->scanTimer = kHazardRescanFrames;
        }

        int const maxDelayFrames = static_cast<int>(
            std::lround(mod->getSettingValue<double>("lookahead-seconds") / jumpcue::kStepSeconds)
        );

        float const stepSpeed = f->speed * jumpcue::kStepSeconds;
        float const lookAhead = stepSpeed * static_cast<float>(maxDelayFrames) + 480.f;
        float const hitboxScale =
            static_cast<float>(mod->getSettingValue<double>("hitbox-scale"));

        std::vector<jumpcue::Rect> boxes;
        boxes.reserve(32);
        for (auto* hazard : f->hazards) {
            if (!hazard || !hazard->getParent() || !hazard->isVisible()) {
                continue;
            }
            float const x = hazard->getPositionX();
            if (x < pos.x - 160.f || x > pos.x + lookAhead) {
                continue;
            }
            jumpcue::Rect const box = toRect(hazard->getObjectRect());
            if (!box.empty()) {
                boxes.push_back(box);
            }
        }

        jumpcue::PlayerState state;
        state.x = pos.x;
        state.y = pos.y;
        state.vx = stepSpeed;
        state.halfW = kPlayerHalfSize * hitboxScale;
        state.halfH = kPlayerHalfSize * hitboxScale;
        state.grounded = grounded;
        state.maxDelayFrames = std::max(8, maxDelayFrames);
        state.arc = f->calibrator.profile();
        if (!state.arc.valid()) {
            state.arc = jumpcue::ArcProfile::fallback();
        }
        f->state = state;

        auto const cue = jumpcue::solve(state, boxes);

        // ---- present ---------------------------------------------------------
        JumpCueHud::Style style;
        style.enabled = true;
        style.spanish = mod->getSettingValue<std::string>("language") != "English";
        style.showSubLabel = mod->getSettingValue<bool>("show-impact");
        style.topPosition = mod->getSettingValue<std::string>("hud-position") == "Arriba";

        if (f->hud) {
            f->hud->refresh(cue, style);
        }

        if (cue.jumpNow && !f->wasJumpNow && f->soundCooldown <= 0.f &&
            mod->getSettingValue<bool>("sound")) {
            playCueSound(static_cast<float>(mod->getSettingValue<double>("volume")));
            f->soundCooldown = 0.12f;
        }
        f->wasJumpNow = cue.jumpNow;
        f->soundCooldown = std::max(0.f, f->soundCooldown - dt);

        ++f->frameCounter;
    }
};

$on_mod(Loaded) {
    log::info("Jump Cue cargado: espera un obstaculo y te avisa cuando saltar.");
}
