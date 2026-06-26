#include "../../Client/Module.hpp"
#include "../Utils/PlayLayer.hpp"
#include "ShowTrajectory/TrajectoryNode.hpp"
#include "CheckpointFix/PlayerState.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <vector>
#include <algorithm>

using namespace geode::prelude;
using namespace qolmod;

// itzz menu: Autoplay (beta)
// Vorausschauender Planer auf Basis der Show-Trajectory-Simulation. Statt stumpf
// "halten vs. loslassen" rechnet er mehrere Sprung-Zeitpunkte durch und waehlt den
// SPAETEST-noetigen Sprung -> springt nur, wenn es ohne Sprung in den Tod ginge.
// Bloecke, Spikes, Pads und Portale werden von der Simulation bereits korrekt
// beruecksichtigt. (Orbs: noch nicht in der Sim -> naechster Ausbauschritt.)

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

static const int AP_H       = 120; // Lookahead in Sim-Schritten (deltaIter=0.5)
static const int AP_SAFE    = 110; // gilt als "sicher", wenn so lange ueberlebt wird
static const int AP_MAXCAND = 14;  // max. gepruefte Sprung-Zeitpunkte (die spaetesten)
static const int AP_HYST    = 10;  // Hysterese fuer Hold-Modi (daempft Flackern)

// Hold-Modi (Dauerdruck): Ship, UFO, Wave, Swing, Robot. Sonst Tap (Cube/Ball/Spider).
static bool apIsHoldMode(PlayerObject* p)
{
    return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing || p->m_isRobot;
}

// Rollt vom aktuellen Klon-Zustand los und liefert die Zahl ueberlebter Schritte.
static int apRollout(TrajectoryNode* tn, bool hold, int steps)
{
    for (int i = 0; i < steps; i++)
    {
        tn->apHold(hold);
        if (!tn->apStep())
            return i;
    }
    return steps;
}

// Entscheidung: Sprung in diesem Frame druecken?
static bool apDecide(PlayerObject* plr, bool holdingNow)
{
    auto tn = TrajectoryNode::get();
    if (!tn || !plr || plr->m_isDead)
        return false;

    tn->apBegin();
    tn->apLoadFrom(plr);

    // --- Hold-Modi (Ship/Wave/Robot/...): halten vs. loslassen mit Hysterese ---
    if (apIsHoldMode(plr))
    {
        PlayerState s0;
        s0.saveState(tn->apClone());

        int rRel = apRollout(tn, false, AP_H);
        if (rRel >= AP_SAFE)
        {
            tn->apEnd();
            return false; // loslassen ist sicher -> nicht halten
        }

        s0.loadState(tn->apClone());
        int rHold = apRollout(tn, true, AP_H);

        tn->apEnd();
        int margin = holdingNow ? -AP_HYST : AP_HYST;
        return rHold > rRel + margin;
    }

    // --- Tap-Modi (Cube/Ball/Spider): spaetest-noetigen Sprung suchen ---
    PlayerState s0;
    s0.saveState(tn->apClone());

    std::vector<int> grounds;           // Frames, in denen gesprungen werden koennte
    int deathStep = AP_H;
    for (int t = 0; t < AP_H; t++)
    {
        if (tn->apClone()->m_isOnGround)
            grounds.push_back(t);

        tn->apHold(false);
        if (!tn->apStep())
        {
            deathStep = t;
            break;
        }
    }

    if (deathStep >= AP_SAFE)
    {
        tn->apEnd();
        return false; // ohne Sprung sicher -> NICHT springen
    }

    // Sprung-Zeitpunkte von spaet nach frueh testen: welcher rettet am laengsten?
    int bestReach = deathStep;
    int bestT = -1;
    int start = std::max(0, (int)grounds.size() - AP_MAXCAND);

    for (int i = (int)grounds.size() - 1; i >= start; i--)
    {
        int t = grounds[i];
        if (t >= deathStep)
            continue;

        s0.loadState(tn->apClone());

        // bis t loslassen
        bool ok = true;
        for (int k = 0; k < t; k++)
        {
            tn->apHold(false);
            if (!tn->apStep()) { ok = false; break; }
        }
        if (!ok)
            continue;

        // an t springen, danach den Rest loslassen
        tn->apHold(true);
        if (!tn->apStep())
            continue; // Sprung hier sofort toedlich

        int reach = t + 1 + apRollout(tn, false, AP_H - t - 1);
        if (reach > bestReach)
        {
            bestReach = reach;
            bestT = t;
        }
        if (bestReach >= AP_SAFE)
            break; // gut genug
    }

    tn->apEnd();

    // Nur druecken, wenn der noetige Sprung GENAU JETZT faellig ist.
    return bestT == 0;
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

        bool want = apDecide(plr, state);
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
