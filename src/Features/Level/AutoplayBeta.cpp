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

// Lookahead-Horizont in Sim-Schritten (deltaIter=0.5). Kuerzer = spaeteres,
// praeziseres Springen (Cube); laenger = mehr Voraussicht (Ship/Wave).
static const int AP_HORIZON     = 140;
static const int AP_SAFE_MARGIN = 14; // gilt als "sicher", wenn so lange ueberlebt wird
static const int AP_HYSTERESIS  = 12; // daempft Hin-und-Her-Kippen

// liefert true, wenn der Sprung (weiter) gehalten werden soll.
// currentlyHolding = aktueller Eingabezustand (fuer Hysterese).
static bool autoplayDecide(PlayerObject* plr, bool currentlyHolding)
{
    auto tn = TrajectoryNode::get();
    if (!tn || !plr || plr->m_isDead)
        return false;

    int safe = AP_HORIZON - AP_SAFE_MARGIN;

    // 1) Loslassen ueberlebt lange genug -> NICHT springen (Cube bleibt am Boden ruhig)
    int sRelease = tn->simulateSurvival(plr, false, AP_HORIZON);
    if (sRelease >= safe)
        return false;

    // 2) Loslassen waere toedlich -> pruefen, ob Halten rettet
    int sHold = tn->simulateSurvival(plr, true, AP_HORIZON);
    if (sHold >= safe)
        return true;

    // 3) Beide sterben -> die deutlich laenger ueberlebende Option waehlen.
    //    Hysterese: am aktuellen Zustand festhalten, solange die Alternative
    //    nicht klar besser ist -> verhindert Frame-genaues Flackern.
    int margin = currentlyHolding ? -AP_HYSTERESIS : AP_HYSTERESIS;
    return sHold > sRelease + margin;
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

        bool want = autoplayDecide(plr, state);
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
