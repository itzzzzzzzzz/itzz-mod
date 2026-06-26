#include "../../Client/Module.hpp"
#include "../Utils/PlayLayer.hpp"
#include "ShowTrajectory/TrajectoryNode.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;
using namespace qolmod;

// itzz menu: Autoplay (beta)
// Baut auf der Show-Trajectory-Vorwaerts-Simulation auf: jeden Physik-Frame wird
// fuer beide Eingaben (gehalten / losgelassen) simuliert, wie lange der Spieler
// ueberlebt. Gewaehlt wird die Eingabe mit dem laengeren Ueberleben -> das Spiel
// findet selbst den richtigen Pfad. Greedy, daher "beta".

class AutoplayBeta : public Module
{
    public:
        MODULE_SETUP(AutoplayBeta)
        {
            setName("Autoplay (beta)");
            setID("autoplay-beta");
            setCategory("Autoplay");
            setSafeModeTrigger(SafeModeTrigger::Attempt);
        }
};

SUBMIT_HACK(AutoplayBeta);

// liefert true, wenn Sprung gehalten werden soll
static bool autoplayDecide(PlayerObject* plr)
{
    auto tn = TrajectoryNode::get();
    if (!tn || !plr || plr->m_isDead)
        return false;

    int sHold    = tn->simulateSurvival(plr, true);
    int sRelease = tn->simulateSurvival(plr, false);

    // bei Gleichstand NICHT springen (haelt den Cube am Boden, Ship faellt ruhig)
    return sHold > sRelease;
}

class $modify(AutoplayBaseGameLayer, GJBaseGameLayer)
{
    struct Fields
    {
        bool p1State = false;
        bool p2State = false;
    };

    void applyAutoplay(PlayerObject* plr, bool isPlayer1, bool& state)
    {
        if (!plr || plr->m_isDead)
            return;

        bool want = autoplayDecide(plr);
        if (want != state)
        {
            this->GJBaseGameLayer::handleButton(want, (int)PlayerButton::Jump, isPlayer1);
            state = want;
        }
    }

    #if GEODE_COMP_GD_VERSION >= 22081
    void processQueuedButtons(float dt, bool clearInputQueue)
    #else
    void processQueuedButtons()
    #endif
    {
        auto pl = PlayLayer::get();

        if (AutoplayBeta::get()->getRealEnabled() && pl && !pl->m_isPaused)
        {
            auto fields = m_fields.self();
            applyAutoplay(m_player1, true, fields->p1State);

            if (m_player2 && m_gameState.m_isDualMode)
                applyAutoplay(m_player2, false, fields->p2State);
        }

        #if GEODE_COMP_GD_VERSION >= 22081
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        #else
        GJBaseGameLayer::processQueuedButtons();
        #endif
    }

    void resetLevelVariables()
    {
        GJBaseGameLayer::resetLevelVariables();

        m_fields->p1State = false;
        m_fields->p2State = false;
    }
};
