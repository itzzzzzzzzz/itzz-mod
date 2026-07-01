#include "../../Client/Module.hpp"
#include "../Utils/PlayLayer.hpp"
#include "ShowTrajectory/TrajectoryNode.hpp"
#include "CheckpointFix/PlayerState.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <vector>
#include <utility>
#include <algorithm>
#include <memory>
#include <fstream>

using namespace geode::prelude;
using namespace qolmod;

// itzz menu: Autoplay (beta)
// Vorausschauender Planer auf Basis der Show-Trajectory-Simulation.
// - Tap-Modi (Cube/Ball/Spider): rekursive Mehrstufen-Suche ueber Sprung-/Orb-Tap-Sequenzen.
//   Orbs werden im Klon DETERMINISTISCH ueber den echten ringJump gefeuert.
// - Hold-Modi (Ship/Wave/Robot/...): Halten vs. Loslassen mit Hysterese (pro Frame).
// Es wird der spaetest-noetige Eingriff gewaehlt -> nur springen/tappen wenn noetig.
// Der gewaehlte Pfad wird magenta visualisiert.

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

// --- Macro: frame-genaues Aufnehmen/Abspielen echter Eingaben (deterministisch) ---
class MacroRecord : public Module
{
    public:
        MODULE_SETUP(MacroRecord)
        {
            setName("Record Macro");
            setID("macro-record");
            setCategory("Autoplay");
        }
};

class MacroReplay : public Module
{
    public:
        MODULE_SETUP(MacroReplay)
        {
            setName("Replay Macro");
            setID("macro-replay");
            setCategory("Autoplay");
        }
};

// Offline-Solver: rechnet das ganze Level per Beam-Suche voraus und baut ein Macro.
class AutoSolve : public Module
{
    public:
        MODULE_SETUP(AutoSolve)
        {
            setName("Auto Solve (exp.)");
            setID("auto-solve");
            setCategory("Autoplay");
        }
};

SUBMIT_HACK(MacroRecord);
SUBMIT_HACK(MacroReplay);
SUBMIT_HACK(AutoSolve);

struct MacroFrame { int frame; bool p1; bool p2; };
static std::vector<MacroFrame> g_macro;
static int    g_frame     = 0;
static size_t g_replayIdx = 0;
static bool   g_repP1 = false, g_repP2 = false; // aktuell abgespielter Zustand
static bool   g_recP1 = false, g_recP2 = false; // zuletzt aufgenommener Zustand
static bool   g_solved = false;                 // Solver hat dieses Level schon berechnet

static std::string macroFile()
{
    return (Mod::get()->getSaveDir() / "macro.gdm").string();
}

static void macroSave()
{
    std::ofstream f(macroFile());
    if (!f) return;
    f << g_macro.size() << "\n";
    for (auto& m : g_macro)
        f << m.frame << " " << (m.p1 ? 1 : 0) << " " << (m.p2 ? 1 : 0) << "\n";
}

static void macroLoad()
{
    std::ifstream f(macroFile());
    if (!f) return;
    g_macro.clear();
    size_t n = 0;
    f >> n;
    for (size_t i = 0; i < n; i++)
    {
        MacroFrame m; int a = 0, b = 0;
        if (!(f >> m.frame >> a >> b)) break;
        m.p1 = a; m.p2 = b;
        g_macro.push_back(m);
    }
}

static const int AP_H       = 150; // Lookahead in Sim-Schritten
static const int AP_DEPTH   = 1;   // Suchtiefe (1 = schlank/fluessig; hoeher = mehr Last)
static const int AP_MAXCAND = 12;  // max. gepruefte Eingriffs-Zeitpunkte pro Ebene
static const int AP_HYST    = 8;   // Hysterese (Hold-Modi)
static const int AP_LATENCY = 1;   // Eingabe-Latenz in Schritten -> einen Tick frueher druecken

static bool apIsHoldMode(PlayerObject* p)
{
    return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing || p->m_isRobot;
}

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

// Crash-sicheres Orb-Velocity-Modell (kein echter Spielcode auf dem Klon).
// Naeherung pro Orb-Typ und Spielmodus.
static void apFireOrb(PlayerObject* p, RingObject* ring)
{
    float yellow = 11.18f;
    float base = yellow;
    if (p->m_isShip)        base = 8.0f;
    else if (p->m_isBall)   base = yellow * 0.7f;
    else if (p->m_isBird)   base = 8.0f;
    else if (p->m_isDart)   base = 6.0f;
    else if (p->m_isRobot)  base = yellow * 0.9f;
    else if (p->m_isSpider) base = yellow * 0.7f;
    else if (p->m_isSwing)  base = yellow * 0.6f;

    float mult = 1.0f;
    bool flip = false;
    switch (ring->m_objectType)
    {
        case GameObjectType::PinkJumpRing: mult = 0.55f; break;
        case GameObjectType::RedJumpRing:  mult = 1.6f;  break;
        case GameObjectType::GravityRing:  flip = true; mult = 0.8f; break; // blau
        case GameObjectType::GreenRing:    flip = true; mult = 1.0f; break;
        case GameObjectType::DropRing:     mult = 1.0f; break;
        case GameObjectType::YellowJumpRing:
        default:                           mult = 1.0f; break;
    }

    if (flip)
        p->m_isUpsideDown = !p->m_isUpsideDown;

    float s = p->m_isUpsideDown ? -1.0f : 1.0f;
    p->m_yVelocity = base * mult * s;
}

// Fuehrt am aktuellen Klon-Zustand den Eingriff aus: Orb feuern bzw. Sprung druecken,
// dann genau EINEN Schritt. true = noch am Leben.
static bool apIntervene(TrajectoryNode* tn, RingObject* ring)
{
    if (ring)
    {
        apFireOrb(tn->apClone(), ring); // sicheres Velocity-Modell
        tn->apHold(false);
    }
    else
    {
        tn->apHold(true);
    }
    return tn->apStep();
}

// Rekursive Suche (Tap-Modi). Klon-Zustand muss vom Aufrufer gesetzt sein.
// Gibt die Zahl ueberlebter Schritte zurueck; outFirstT = Frame des ersten Eingriffs (-1 = keiner).
static int apSearchTap(TrajectoryNode* tn, int horizon, int depth, int& outFirstT)
{
    outFirstT = -1;

    PlayerState base;
    base.saveState(tn->apClone());

    // Release-Rollout: Kandidaten (Boden/Orb) sammeln, Todeszeitpunkt finden
    std::vector<std::pair<int, RingObject*>> cand;
    int deathRel = horizon;
    for (int t = 0; t < horizon; t++)
    {
        bool ground = tn->apClone()->m_isOnGround;
        tn->apHold(false);
        bool alive = tn->apStep();
        RingObject* orb = tn->apOrbThisStep;

        if (orb)        cand.push_back({ t, orb });
        else if (ground) cand.push_back({ t, nullptr });

        if (!alive) { deathRel = t; break; }
    }

    if (deathRel >= horizon)
        return horizon; // Release ist sicher -> kein Eingriff noetig

    int best = deathRel;
    int startIdx = std::max(0, (int)cand.size() - AP_MAXCAND);

    for (int i = (int)cand.size() - 1; i >= startIdx; i--)
    {
        int t = cand[i].first;
        RingObject* ring = cand[i].second;

        base.loadState(tn->apClone());

        bool ok = true;
        for (int k = 0; k < t; k++)
        {
            tn->apHold(false);
            if (!tn->apStep()) { ok = false; break; }
        }
        if (!ok)
            continue;

        if (!apIntervene(tn, ring))
            continue; // Eingriff sofort toedlich

        int after = t + 1;
        int reach;
        if (depth <= 1)
        {
            reach = after + apRollout(tn, false, horizon - after);
        }
        else
        {
            int subT;
            reach = after + apSearchTap(tn, horizon - after, depth - 1, subT);
        }

        if (reach > best) { best = reach; outFirstT = t; }
        if (best >= horizon) break;
    }

    return best;
}

// Entscheidung: in diesem Frame druecken?
static bool apDecide(PlayerObject* plr, bool holdingNow, bool recordViz)
{
    auto tn = TrajectoryNode::get();
    if (!tn || !plr || plr->m_isDead)
        return false;

    tn->apBegin();
    PlayerState s0;
    tn->apLoadFrom(plr);
    s0.saveState(tn->apClone());

    bool press = false;
    int  vizFirstT = -1;
    bool vizRing = false;

    if (!apIsHoldMode(plr))
    {
        // --- Tap-Modi: rekursive Sequenz-Suche ---
        int firstT;
        int reach = apSearchTap(tn, AP_H, AP_DEPTH, firstT);
        (void)reach;
        // Latenz-Kompensation: einen Tick frueher druecken (gegen "zu spaet geklickt")
        press = (firstT >= 0 && firstT <= AP_LATENCY);
        vizFirstT = firstT;

        // fuer die Visualisierung: war der erste Eingriff ein Orb?
        // (firstT-Kandidat erneut bestimmen ist teuer; wir markieren generisch)
    }
    else
    {
        // --- Hold-Modi: Halten vs. Loslassen mit Hysterese ---
        int reachRel = apRollout(tn, false, AP_H);
        if (reachRel < AP_H - (AP_H / 12))
        {
            s0.loadState(tn->apClone());
            int reachHold = apRollout(tn, true, AP_H);
            int margin = holdingNow ? -AP_HYST : AP_HYST;
            press = reachHold > reachRel + margin;
        }
        vizFirstT = press ? 0 : -1;
    }

    // --- Visualisierung des gewaehlten ersten Schritts ---
    if (recordViz)
    {
        tn->apPath.clear();
        s0.loadState(tn->apClone());
        tn->apPath.push_back(tn->apClone()->getPosition());

        bool holdMode = apIsHoldMode(plr);
        for (int t = 0; t < AP_H; t++)
        {
            bool h;
            if (holdMode)
                h = press; // Hold-Modus: Eingabe konstant darstellen
            else
                h = (t == vizFirstT); // Tap-Modus: ein Druck am gewaehlten Frame

            tn->apHold(h);
            if (!tn->apStep())
                break;
            tn->apPath.push_back(tn->apClone()->getPosition());
        }
        (void)vizRing;
    }

    tn->apEnd();
    return press;
}

// Offline-Beam-Suche: rechnet das ganze Level voraus und baut g_macro (frame-genau).
// Exakt fuer Sprung/Block/Spike/Pad/Portal-Level (Sim = echte Physik). Orbs: Naeherung.
static void apSolve(PlayerObject* startPlr)
{
    auto tn = TrajectoryNode::get();
    g_macro.clear();
    if (!tn || !startPlr)
        return;

    const int  MAXF   = 30000;   // max. Frames Vorausrechnung
    const int  BEAM   = 16;      // parallel verfolgte beste Pfade
    const long BUDGET = 900000;  // max. Simulationsschritte (begrenzt die Freeze-Zeit)

    // PlayerState hinter unique_ptr: Android-gnustl kann gd::map nicht move-assignen,
    // so werden beim Sortieren/Verschieben nur Zeiger bewegt.
    struct Node { std::unique_ptr<PlayerState> st; std::vector<char> hist; float x = 0; };

    tn->apBegin();
    tn->apLoadFrom(startPlr);

    std::vector<Node> frontier;
    {
        Node n;
        n.st = std::make_unique<PlayerState>();
        n.st->saveState(tn->apClone());
        n.x = tn->apClone()->getPositionX();
        frontier.push_back(std::move(n));
    }

    std::vector<char> bestHist = frontier[0].hist;
    float bestX = frontier[0].x;
    long steps = 0;

    for (int f = 0; f < MAXF && steps < BUDGET && !frontier.empty(); f++)
    {
        std::vector<Node> children;
        children.reserve(frontier.size() * 2);

        for (auto& s : frontier)
        {
            for (int inp = 0; inp < 2; inp++)
            {
                s.st->loadState(tn->apClone());
                tn->apHold(inp != 0);
                bool alive = tn->apStep();
                steps++;
                if (!alive)
                    continue;

                Node c;
                c.st = std::make_unique<PlayerState>();
                c.st->saveState(tn->apClone());
                c.x = tn->apClone()->getPositionX();
                c.hist = s.hist;
                c.hist.push_back((char)inp);
                children.push_back(std::move(c));
            }
        }

        if (children.empty())
            break; // alle Pfade tot

        std::sort(children.begin(), children.end(),
                  [](const Node& a, const Node& b) { return a.x > b.x; });
        if ((int)children.size() > BEAM)
            children.resize(BEAM);

        frontier = std::move(children);
        if (frontier[0].x > bestX)
        {
            bestX = frontier[0].x;
            bestHist = frontier[0].hist;
        }
    }

    tn->apEnd();

    // besten Pfad -> Macro (Kanten des Eingabezustands)
    g_macro.clear();
    bool last = false;
    for (size_t i = 0; i < bestHist.size(); i++)
    {
        bool p1 = bestHist[i] != 0;
        if (i == 0 || p1 != last)
        {
            g_macro.push_back({ (int)i, p1, false });
            last = p1;
        }
    }
}

class $modify(AutoplayBaseGameLayer, GJBaseGameLayer)
{
    struct Fields
    {
        bool p1State = false;
        bool p2State = false;
    };

    void applyAutoplay(PlayerObject* plr, bool isPlayer1, bool& state, bool recordViz)
    {
        if (!plr || plr->m_isDead)
            return;

        bool want = apDecide(plr, state, recordViz);
        if (want != state)
        {
            this->GJBaseGameLayer::handleButton(want, (int)PlayerButton::Jump, isPlayer1);
            state = want;
        }
    }

    // Macro abspielen: aufgenommene Eingaben fuer den aktuellen Frame anwenden
    void macroApplyReplay()
    {
        while (g_replayIdx < g_macro.size() && g_macro[g_replayIdx].frame <= g_frame)
        {
            auto& m = g_macro[g_replayIdx];
            if (m.p1 != g_repP1) { this->GJBaseGameLayer::handleButton(m.p1, (int)PlayerButton::Jump, true);  g_repP1 = m.p1; }
            if (m.p2 != g_repP2) { this->GJBaseGameLayer::handleButton(m.p2, (int)PlayerButton::Jump, false); g_repP2 = m.p2; }
            g_replayIdx++;
        }
    }

    // Macro aufnehmen: tatsaechlichen Eingabezustand dieses Frames festhalten
    void macroRecordFrame()
    {
        bool p1 = m_player1 && m_player1->m_holdingButtons[(int)PlayerButton::Jump];
        bool p2 = m_player2 && m_player2->m_holdingButtons[(int)PlayerButton::Jump];
        if (g_macro.empty() || p1 != g_recP1 || p2 != g_recP2)
        {
            g_macro.push_back({ g_frame, p1, p2 });
            g_recP1 = p1; g_recP2 = p2;
        }
    }

    #if GEODE_COMP_GD_VERSION >= 22081
    void processQueuedButtons(float dt, bool clearInputQueue)
    #else
    void processQueuedButtons()
    #endif
    {
        auto pl = PlayLayer::get();
        auto tn = TrajectoryNode::get();
        bool playing = pl && !pl->m_isPaused;

        bool autoOn  = AutoplayBeta::get()->getRealEnabled();
        bool recOn   = MacroRecord::get()->getRealEnabled();
        bool repOn   = MacroReplay::get()->getRealEnabled();
        bool solveOn = AutoSolve::get()->getRealEnabled();

        if (!solveOn)
            g_solved = false;

        if (tn)
            tn->apDrawPath = autoOn && playing && !repOn && !solveOn;

        if (playing)
        {
            auto fields = m_fields.self();

            if ((repOn || (solveOn && g_solved)) && !g_macro.empty())
            {
                macroApplyReplay();
            }
            else if (autoOn)
            {
                applyAutoplay(m_player1, true, fields->p1State, true);
                if (m_player2 && m_gameState.m_isDualMode)
                    applyAutoplay(m_player2, false, fields->p2State, false);
            }

            if (recOn)
                macroRecordFrame();
        }

        #if GEODE_COMP_GD_VERSION >= 22081
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        #else
        GJBaseGameLayer::processQueuedButtons();
        #endif

        if (playing)
            g_frame++;
    }

    void resetLevelVariables()
    {
        GJBaseGameLayer::resetLevelVariables();

        m_fields->p1State = false;
        m_fields->p2State = false;

        g_frame = 0;
        g_replayIdx = 0;
        g_repP1 = g_repP2 = false;
        g_recP1 = g_recP2 = false;

        if (AutoSolve::get()->getRealEnabled())
        {
            if (!g_solved)
            {
                apSolve(m_player1);      // ganzes Level vorausrechnen (blockiert kurz)
                if (!g_macro.empty())    // nur markieren, wenn wirklich berechnet
                    g_solved = true;
            }
        }
        else if (MacroReplay::get()->getRealEnabled())
            macroLoad();                 // Aufnahme fuer diesen Versuch laden
        else if (MacroRecord::get()->getRealEnabled())
            g_macro.clear();             // neue Aufnahme beginnen
    }
};

// Bei Levelabschluss waehrend der Aufnahme das (erfolgreiche) Macro speichern
class $modify(MacroPlayLayer, PlayLayer)
{
    void levelComplete()
    {
        PlayLayer::levelComplete();

        if (MacroRecord::get()->getRealEnabled() && !g_macro.empty())
            macroSave();
    }
};
