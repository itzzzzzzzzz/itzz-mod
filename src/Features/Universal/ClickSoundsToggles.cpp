#include "../../Client/Module.hpp"
#include "../../Client/EnumModule.hpp"

using namespace geode::prelude;

// itzz menu: EIGENE (standalone) Click-Sounds-Toggles + Pack-Auswahl.

#define CS_BOOL(klass, modName, modId, def)         \
class klass : public Module                         \
{                                                   \
    public:                                         \
        MODULE_SETUP(klass)                         \
        {                                           \
            setName(modName);                       \
            setID(modId);                           \
            setCategory("Click Sounds");           \
            setDefaultEnabled(def);                 \
        }                                           \
};                                                  \
SUBMIT_HACK(klass);

CS_BOOL(CSMaster,     "Master Enable",         "cs-master",     true)
CS_BOOL(CSClicks,     "Enable Click Sounds",   "cs-clicks",     true)
CS_BOOL(CSReleases,   "Enable Release Sounds", "cs-releases",   true)
CS_BOOL(CSEverywhere, "Sounds Everywhere",     "cs-everywhere", false)
CS_BOOL(CSOnJump,     "Only On Jump",          "cs-onjump",     true)

// Pack-Auswahl (generische Namen, keine Sound-Namen)
enum class ItzzClickPack { All, Pack1, Pack2, Pack3, Pack4 };

class ClickSoundPack : public EnumModule
{
    public:
        MODULE_SETUP(ClickSoundPack)
        {
            setName("Sound Pack");
            setID("cs-pack");
            setCategory("Click Sounds");
            setDescription("Choose which click sound pack to use");
            initValues<ItzzClickPack>();
            setDefaultValue(0);
        }
};

SUBMIT_HACK(ClickSoundPack);
