// Input Buffer - Geode mod
// If you click a little too early (before landing), the click is held for a
// short window and fires as soon as you touch the ground.
//
// UNTESTED: written without being able to compile against 2.2081 bindings.
// If a member name fails to compile, check it in the Geode bindings.

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

// Only buffer for modes where a click means "jump from the ground".
static bool groundMode(PlayerObject* p) {
    return !p->m_isShip && !p->m_isBird && !p->m_isDart && !p->m_isSwing;
}

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        float timeLeft[2] = {0.f, 0.f};   // remaining buffer time per player
        bool held[2] = {false, false};    // is the real button currently down
        bool releasePending[2] = {false, false};
        bool injecting = false;           // stops our own clicks re-arming the buffer
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        auto f = m_fields.self();
        if (f->injecting || button != 1) return; // 1 = jump button

        int i = isPlayer1 ? 0 : 1;
        f->held[i] = down;

        if (down && Mod::get()->getSettingValue<bool>("enabled")) {
            f->timeLeft[i] = Mod::get()->getSettingValue<int64_t>("buffer-ms") / 1000.f;
        }
    }

    void processCommands(float dt) {
        GJBaseGameLayer::processCommands(dt);

        auto f = m_fields.self();
        if (!Mod::get()->getSettingValue<bool>("enabled")) return;

        for (int i = 0; i < 2; i++) {
            PlayerObject* p = (i == 0) ? m_player1 : m_player2;
            bool isP1 = (i == 0);
            if (!p || p->m_isDead) {
                f->timeLeft[i] = 0.f;
                f->releasePending[i] = false;
                continue;
            }

            // Release the click we injected on the previous step.
            if (f->releasePending[i]) {
                f->injecting = true;
                GJBaseGameLayer::handleButton(false, 1, isP1);
                f->injecting = false;
                f->releasePending[i] = false;
                continue;
            }

            if (f->timeLeft[i] <= 0.f) continue;
            f->timeLeft[i] -= dt;

            // Real button still down: the game's normal hold-to-rejump handles it.
            if (f->held[i]) continue;

            // Click was released early; fire it now that we're on the ground.
            if (p->m_isOnGround && groundMode(p)) {
                f->injecting = true;
                GJBaseGameLayer::handleButton(true, 1, isP1);
                f->injecting = false;
                f->releasePending[i] = true;
                f->timeLeft[i] = 0.f;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Press V to open a small window where you can enable/disable the buffer.
// ---------------------------------------------------------------------------

class BufferToggleDelegate : public FLAlertLayerProtocol {
public:
    static inline bool open = false;

    static BufferToggleDelegate* get() {
        static BufferToggleDelegate inst;
        return &inst;
    }

    static void show() {
        if (open) return;
        open = true;

        bool on = Mod::get()->getSettingValue<bool>("enabled");
        std::string body = on
            ? "Input Buffer is <cg>ON</c>."
            : "Input Buffer is <cr>OFF</c>.";

        auto alert = FLAlertLayer::create(
            get(),
            "Input Buffer",
            body,
            "Close",
            on ? "Disable" : "Enable"
        );
        alert->m_scene = CCDirector::sharedDirector()->getRunningScene();
        alert->show();
    }

    void FLAlert_Clicked(FLAlertLayer*, bool btn2) override {
        open = false;
        if (btn2) {
            bool on = Mod::get()->getSettingValue<bool>("enabled");
            Mod::get()->setSettingValue<bool>("enabled", !on);
        }
    }
};

class $modify(KeyHook, CCKeyboardDispatcher) {
    // If this fails to compile, check the signature in the bindings;
    // on 2.2081 it should take a timestamp as the last parameter.
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && key == KEY_V) {
            BufferToggleDelegate::show();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};
