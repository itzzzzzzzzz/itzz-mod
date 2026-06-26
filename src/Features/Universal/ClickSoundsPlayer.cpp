#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "../../Client/Module.hpp"
#include "../../Client/EnumModule.hpp"
#include "../Level/ShowTrajectory/TrajectoryNode.hpp"
#include <cstdlib>
#include <unordered_map>

using namespace geode::prelude;

// itzz menu: standalone Click Sounds.
// Low-Level-FMOD (createSound + playSound mit Cache) wie beat.click-sound -> zuverlaessig.

// Sound-Anzahl pro Pack (Index 1..4); 0 = "All"
static const int CS_PACK_CLICK[5]   = {0, 5, 25, 16, 1};
static const int CS_PACK_RELEASE[5] = {0, 5, 25, 16, 1};

static int csSelectedPack()
{
    if (auto m = Module::getByID("cs-pack"))
        return static_cast<EnumModule*>(m)->getValue();
    return 0;
}

static bool csOn(const char* id)
{
    auto m = Module::getByID(id);
    return m && m->getRealEnabled();
}

static bool csInLevelOrEditor()
{
    if (auto pl = PlayLayer::get())
        return !pl->m_isPaused;
    if (LevelEditorLayer::get())
        return true;
    return false;
}

static bool csShouldPlay(PlayerButton btn)
{
    // Kein Sound waehrend der Trajectory-/Autoplay-Simulation (Klon-Spieler)
    if (qolmod::TrajectoryNode::get() && qolmod::TrajectoryNode::get()->isSimulating())
        return false;
    if (!csOn("cs-master"))
        return false;
    if (!csOn("cs-everywhere") && !csInLevelOrEditor())
        return false;
    if (csOn("cs-onjump") && btn != PlayerButton::Jump)
        return false;
    return true;
}

static FMOD::Sound* csGetSound(const std::string& path)
{
    static std::unordered_map<std::string, FMOD::Sound*> cache;
    auto it = cache.find(path);
    if (it != cache.end())
        return it->second;

    FMOD::Sound* snd = nullptr;
    auto eng = FMODAudioEngine::get();
    if (eng && eng->m_system)
    {
        if (eng->m_system->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &snd) == FMOD_OK)
            snd->setMode(FMOD_LOOP_OFF);
        else
            snd = nullptr;
    }
    cache[path] = snd;
    return snd;
}

static void csPlay(bool isClick, float vol)
{
    int pack = csSelectedPack();          // 0 = All, 1..4 = Pack
    if (pack < 0 || pack > 4)
        pack = 0;
    if (pack == 0)
        pack = (std::rand() % 4) + 1;     // All -> zufaelliger Pack

    int count = isClick ? CS_PACK_CLICK[pack] : CS_PACK_RELEASE[pack];
    if (count < 1)
        count = 1;
    int n = (std::rand() % count) + 1;
    const char* type = isClick ? "click" : "release";
    auto path = (Mod::get()->getResourcesDir() / fmt::format("csp{}_{}_{}.ogg", pack, type, n)).string();

    auto eng = FMODAudioEngine::get();
    auto snd = csGetSound(path);
    if (eng && eng->m_system && snd)
    {
        FMOD::Channel* ch = nullptr;
        eng->m_system->playSound(snd, nullptr, false, &ch);
        if (ch)
            ch->setVolume(vol);
    }
}

class $modify(itzzClickSoundsHook, PlayerObject)
{
    bool pushButton(PlayerButton p0)
    {
        bool ret = PlayerObject::pushButton(p0);
        if (csShouldPlay(p0) && csOn("cs-clicks"))
            csPlay(true, 0.6f);
        return ret;
    }

    bool releaseButton(PlayerButton p0)
    {
        bool ret = PlayerObject::releaseButton(p0);
        if (csShouldPlay(p0) && csOn("cs-releases"))
            csPlay(false, 0.6f);
        return ret;
    }
};
