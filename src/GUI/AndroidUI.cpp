#include "AndroidUI.hpp"
#include <Geode/ui/ScrollLayer.hpp>
#include "FloatingButton/FloatingUIManager.hpp"
#include "AndroidBall.hpp"
#include "BetterSlider.hpp"
#include "../Features/Speedhack/Speedhack.hpp"
#include <Gestures/GestureManager.hpp>
#include "Modules/DisableOpenInLevel.hpp"
#include <algorithm>
#include <cctype>
#include <vector>

using namespace geode::prelude;
using namespace qolmod;

// itzz: minimalistisches UI (Lexend, off-white Panel, graue Checkboxen)

static const std::vector<std::string> ITZZ_CATS = {
    "Level", "Universal", "Click Sounds", "Icon", "Speedhack"
};

static std::string itzzUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

static const ccColor3B ITZZ_TEXT  = ccc3(140, 140, 137);
static const ccColor3B ITZZ_TEXTD = ccc3(90, 90, 88);

bool AndroidUI::setup()
{
    instance = this;
    float w = m_size.width;
    float h = m_size.height;

    m_bgSprite->setVisible(false);
    m_buttonMenu->setVisible(false);

    // off-white Panel (deckend, keine Scale9-Fragmente)
    auto panel = CCScale9Sprite::create("square.png");
    panel->setColor(ccc3(250, 250, 248));
    panel->setContentSize(m_size);
    panel->setAnchorPoint(ccp(0, 0));
    m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, ccp(0, 0));

    // duenne Trennlinie zwischen Sidebar und Inhalt
    auto divider = CCLayerColor::create(ccc4(227, 227, 224, 255), 1.f, h - 30.f);
    m_mainLayer->addChildAtPosition(divider, Anchor::BottomLeft, ccp(140, 15));

    // Chrome-Logo in der oberen Leiste (Fallback: Text)
    auto logo = CCSprite::create((std::string(GEODE_MOD_ID) + "/itzz-logo.png").c_str());
    if (logo && logo->getContentSize().height > 0)
    {
        logo->setScale(26.f / logo->getContentSize().height);
        m_mainLayer->addChildAtPosition(logo, Anchor::BottomLeft, ccp(w / 2.f, h - 16.f));
    }
    else
    {
        auto title = CCLabelBMFont::create("ITZZ", "lexend.fnt"_spr);
        title->setColor(ITZZ_TEXTD);
        title->setScale(0.42f);
        m_mainLayer->addChildAtPosition(title, Anchor::BottomLeft, ccp(w / 2.f, h - 14.f));
    }

    itzzBuildSidebar();
    itzzRebuildList();
    return true;
}

void AndroidUI::itzzBuildSidebar()
{
    if (m_sideMenu)
        m_sideMenu->removeFromParent();

    m_sideMenu = CCMenu::create();
    m_sideMenu->setContentSize(m_size);
    m_sideMenu->setAnchorPoint(ccp(0, 0));
    m_sideMenu->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(m_sideMenu, Anchor::BottomLeft, ccp(0, 0));

    static const std::vector<std::pair<std::string, std::string>> icons = {
        {"Level", "yeezy_ic_level_play.png"},
        {"Universal", "yeezy_ic_universal_globe.png"},
        {"Click Sounds", "yeezy_ic_cosmetic_sparkle.png"},
        {"Icon", "yeezy_ic_icon_effects_key.png"},
        {"Speedhack", "yeezy_ic_speed_clock.png"},
    };

    float top = m_size.height - 52.f;
    float gap = 26.f;
    int idx = 0;

    for (auto& cat : ITZZ_CATS)
    {
        bool active = (cat == selectedCategory);

        auto container = CCNode::create();
        container->setContentSize(ccp(128, 24));
        container->setAnchorPoint(ccp(0.5f, 0.5f));

        if (active)
        {
            auto hl = CCScale9Sprite::create("square02b_small.png");
            if (!hl)
                hl = CCScale9Sprite::create("square.png");
            hl->setColor(ccc3(243, 243, 241));
            hl->setContentSize(ccp(128, 24));
            container->addChildAtPosition(hl, Anchor::Center);
        }

        std::string iconFile;
        for (auto& p : icons)
            if (p.first == cat)
                iconFile = p.second;

        if (!iconFile.empty())
        {
            auto ic = CCSprite::create((std::string(GEODE_MOD_ID) + "/" + iconFile).c_str());
            if (ic)
            {
                ic->setColor(ccc3(154, 154, 150));
                ic->setScale(0.16f);
                ic->setPosition(ccp(14, 12));
                container->addChild(ic);
            }
        }

        auto lbl = CCLabelBMFont::create(itzzUpper(cat).c_str(), "lexend.fnt"_spr);
        lbl->setColor(active ? ITZZ_TEXTD : ITZZ_TEXT);
        lbl->setScale(0.3f);
        lbl->setAnchorPoint(ccp(0, 0.5f));
        lbl->setPosition(ccp(28, 12));
        container->addChild(lbl);

        auto btn = CCMenuItemSpriteExtra::create(container, this, menu_selector(AndroidUI::itzzOnCategory));
        btn->setUserObject(CCString::create(cat));
        btn->setPosition(ccp(72, top - idx * gap));
        m_sideMenu->addChild(btn);
        idx++;
    }
}

void AndroidUI::itzzRebuildList()
{
    if (m_list)
        m_list->removeFromParent();
    m_speedSlider = nullptr;
    m_speedVal = nullptr;

    float lx = 148.f;
    float ly = 14.f;
    float lw = m_size.width - lx - 12.f;
    float lh = m_size.height - 34.f;

    m_list = geode::ScrollLayer::create(CCSize(lw, lh));
    m_list->setAnchorPoint(ccp(0, 0));
    m_mainLayer->addChildAtPosition(m_list, Anchor::BottomLeft, ccp(lx, ly));

    std::vector<Module*> mods;
    for (auto m : Module::getAll())
    {
        if (!m || m->getParent() || m->getCategory() != selectedCategory)
            continue;
        mods.push_back(m);
    }

    // Advanced-Kategorien (keine normale Modul-Liste) -> gezielt einblenden
    if (selectedCategory == "Icon")
    {
        if (auto m = Module::getByID("cube-icon-override"))
            mods.push_back(m);
    }
    else if (selectedCategory == "Speedhack")
    {
        for (const char* id : {"speedhack/enabled", "speedhack/music", "speedhack/gameplay"})
            if (auto m = Module::getByID(id))
                mods.push_back(m);
    }

    float rowH = 27.f;
    float headerH = (selectedCategory == "Icon") ? 132.f : 0.f;       // Vorschau oben
    float footerH = (selectedCategory == "Speedhack") ? 78.f : 0.f;   // Slider unten
    float totalH = std::max(lh, mods.size() * rowH + 6.f + headerH + footerH);
    m_list->m_contentLayer->setContentSize(CCSize(lw, totalH));

    auto rowMenu = CCMenu::create();
    rowMenu->setContentSize(CCSize(lw, totalH));
    rowMenu->setAnchorPoint(ccp(0, 0));
    rowMenu->ignoreAnchorPointForPosition(false);
    rowMenu->setPosition(0, 0);
    m_list->m_contentLayer->addChild(rowMenu);

    // --- Icon-Vorschau: rotierender Stern in einer Box ---
    if (selectedCategory == "Icon")
    {
        float cy = totalH - headerH / 2.f + 6.f;
        auto box = CCScale9Sprite::create("square02b_small.png");
        if (box)
        {
            box->setColor(ccc3(243, 243, 241));
            box->setContentSize(ccp(lw - 24.f, headerH - 30.f));
            box->setPosition(ccp(lw / 2.f, cy));
            m_list->m_contentLayer->addChild(box);
        }

        auto star = CCSprite::createWithSpriteFrameName("itzz-star.png"_spr);
        if (!star)
            star = CCSprite::create((std::string(GEODE_MOD_ID) + "/itzz-star.png").c_str());
        if (star && star->getContentSize().height > 0)
        {
            star->setScale((headerH - 64.f) / star->getContentSize().height);
            star->setPosition(ccp(lw / 2.f, cy));
            star->runAction(CCRepeatForever::create(CCRotateBy::create(3.f, 360.f)));
            m_list->m_contentLayer->addChild(star);
        }

        auto cap = CCLabelBMFont::create("CUBE ICON PREVIEW", "lexend.fnt"_spr);
        cap->setColor(ITZZ_TEXTD);
        cap->setScale(0.26f);
        cap->setPosition(ccp(lw / 2.f, totalH - headerH + 14.f));
        m_list->m_contentLayer->addChild(cap);
    }

    float y = totalH - headerH - rowH / 2.f - 3.f;
    for (auto m : mods)
    {
        auto off = CCSprite::createWithSpriteFrameName("yz_checkbox_empty.png"_spr);
        auto on = CCSprite::createWithSpriteFrameName("yz_checkbox_checked.png"_spr);
        CCMenuItemToggler* tog = nullptr;
        if (off && on)
            tog = CCMenuItemToggler::create(off, on, this, menu_selector(AndroidUI::itzzOnToggle));
        else
            tog = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(AndroidUI::itzzOnToggle), 0.6f);
        tog->setScale(0.3f);
        tog->toggle(m->getRealEnabled());
        tog->setUserObject(CCString::create(m->getID()));
        tog->setPosition(ccp(18.f, y));
        rowMenu->addChild(tog);

        auto lbl = CCLabelBMFont::create(itzzUpper(m->getName()).c_str(), "lexend.fnt"_spr);
        lbl->setColor(ITZZ_TEXT);
        lbl->setScale(0.32f);
        lbl->setAnchorPoint(ccp(0, 0.5f));
        lbl->setPosition(ccp(36.f, y));
        lbl->limitLabelWidth(lw - 50.f, 0.32f, 0.1f);
        m_list->m_contentLayer->addChild(lbl);

        y -= rowH;
    }

    // --- Speedhack: Slider fuer den Geschwindigkeits-Multiplikator ---
    if (selectedCategory == "Speedhack")
    {
        auto sh = Speedhack::get();
        float sy = y - 4.f;

        auto cap = CCLabelBMFont::create("SPEED MULTIPLIER", "lexend.fnt"_spr);
        cap->setColor(ITZZ_TEXTD);
        cap->setScale(0.26f);
        cap->setAnchorPoint(ccp(0, 0.5f));
        cap->setPosition(ccp(18.f, sy));
        m_list->m_contentLayer->addChild(cap);

        m_speedVal = CCLabelBMFont::create(fmt::format("{:.2f}x", sh->getValue()).c_str(), "lexend.fnt"_spr);
        m_speedVal->setColor(ITZZ_TEXT);
        m_speedVal->setScale(0.3f);
        m_speedVal->setAnchorPoint(ccp(1, 0.5f));
        m_speedVal->setPosition(ccp(lw - 14.f, sy));
        m_list->m_contentLayer->addChild(m_speedVal);

        m_speedSlider = BetterSlider::create(this, menu_selector(AndroidUI::itzzOnSlider));
        m_speedSlider->setRange(0.1f, 3.0f);
        m_speedSlider->setSnapValuesRanged({ 1.0f });
        m_speedSlider->setValueRanged(sh->getValue());
        m_speedSlider->setScale(0.82f);
        m_speedSlider->setPosition(ccp(lw / 2.f, sy - 26.f));
        m_list->m_contentLayer->addChild(m_speedSlider);
    }

    m_list->moveToTop();
}

void AndroidUI::itzzOnCategory(CCObject* sender)
{
    auto node = static_cast<CCNode*>(sender);
    auto s = static_cast<CCString*>(node->getUserObject());
    if (!s)
        return;
    selectedCategory = s->getCString();
    itzzBuildSidebar();
    itzzRebuildList();
}

void AndroidUI::itzzOnToggle(CCObject* sender)
{
    auto node = static_cast<CCNode*>(sender);
    auto s = static_cast<CCString*>(node->getUserObject());
    if (!s)
        return;
    if (auto m = Module::getByID(s->getCString()))
        m->setUserEnabled(!m->getRealEnabled());
}

void AndroidUI::itzzOnSlider(CCObject* sender)
{
    if (!m_speedSlider)
        return;
    float v = m_speedSlider->getValueRanged();
    Speedhack::get()->setText(fmt::format("{:.02f}", v));
    if (m_speedVal)
        m_speedVal->setString(fmt::format("{:.2f}x", v).c_str());
}

// --- alte Methoden: neutralisiert ---
void AndroidUI::populateModules() {}
void AndroidUI::populateTabs() {}
void AndroidUI::updateTabs() {}
void AndroidUI::switchTabTemp(std::string tab) {}

AndroidUI* AndroidUI::create()
{
    auto pRet = new AndroidUI();
    if (pRet && pRet->initAnchored(475.f, 280.f))
    {
        PlatformToolbox::showCursor();
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return nullptr;
}

AndroidUI* AndroidUI::addToScene()
{
    if (DisableOpenInLevel::get()->getRealEnabled())
    {
        if (PlayLayer::get() && !(PlayLayer::get()->m_isPaused || PlayLayer::get()->m_levelEndAnimationStarted))
            return nullptr;
    }

    auto pRet = create();
    pRet->show();
    return pRet;
}

AndroidUI* AndroidUI::get()
{
    return instance;
}

AndroidUI::~AndroidUI()
{
    if (instance == this)
        instance = nullptr;
    (void)Mod::get()->saveData();
}

void AndroidUI::close()
{
    if (PlayLayer::get() && !PlayLayer::get()->m_isPaused && !PlayLayer::get()->m_levelEndAnimationStarted && !GameManager::sharedState()->getGameVariable("0024"))
        PlatformToolbox::hideCursor();

    this->onClose(nullptr);
    instance = nullptr;
}

void AndroidUI::keyDown(cocos2d::enumKeyCodes key, double timestamp)
{
    PopupBase::keyDown(key, timestamp);
}

void AndroidUI::visit()
{
    FloatingUIManager::get()->visit();
    AndroidBall::get()->visit();
    GestureManager::get()->visit();
    PopupBase::visit();
}

bool AndroidUI::altClickBegan(int button, cocos2d::CCPoint point)
{
    return false;
}
