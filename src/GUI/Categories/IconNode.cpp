#include "IconNode.hpp"

bool IconNode::init()
{
    if (!CategoryNode::init())
        return false;

    auto centre = getContentSize() / 2;

    // Kopf: "Icon" + kleines "Beta"
    auto title = CCLabelBMFont::create("Icon", "bigFont.fnt");
    title->setColor(ccc3(255, 255, 255));
    title->setScale(0.8f);
    title->setAnchorPoint(ccp(0, 0.5f));

    auto beta = CCLabelBMFont::create("Beta", "bigFont.fnt");
    beta->setColor(ccc3(140, 140, 140));
    beta->setScale(0.4f);
    beta->setAnchorPoint(ccp(0, 0.5f));

    float titleW = title->getContentWidth() * title->getScale();
    title->setPosition(centre + ccp(-titleW / 2 - 6, 95));
    beta->setPosition(title->getPosition() + ccp(titleW + 6, -2));

    this->addChild(title);
    this->addChild(beta);

    // Abschnitt "Preloads"
    auto section = CCLabelBMFont::create("Preloads", "goldFont.fnt");
    section->setScale(0.55f);
    section->setAnchorPoint(ccp(0.5f, 0.5f));
    section->setPosition(centre + ccp(0, 62));
    this->addChild(section);

    // viereckige Box: weisser Rand + schwarze Fuellung
    float box = 120.f;
    auto outline = CCScale9Sprite::create("square.png");
    outline->setContentSize(ccp(box + 4, box + 4));
    outline->setColor(ccc3(255, 255, 255));
    outline->setPosition(centre + ccp(0, -5));
    this->addChild(outline);

    auto fill = CCScale9Sprite::create("square.png");
    fill->setContentSize(ccp(box, box));
    fill->setColor(ccc3(0, 0, 0));
    fill->setPosition(centre + ccp(0, -5));
    this->addChild(fill);

    // rotierender Stern in der Box
    if (auto star = CCSprite::create("itzz-star.png"_spr))
    {
        star->setScale(0.6f);
        star->setPosition(fill->getPosition());
        star->runAction(CCRepeatForever::create(CCRotateBy::create(2.5f, 360.f)));
        this->addChild(star);
    }

    // Enable/Disable: Cube-Icon mit Stern ueberschreiben
    auto menu = CCMenu::create();
    menu->setPosition(0, 0);
    this->addChild(menu);

    auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(IconNode::onToggleOverride), 0.7f);
    toggler->setPosition(centre + ccp(-78, -82));
    if (auto m = Module::getByID("cube-icon-override"))
        toggler->toggle(m->getRealEnabled());
    menu->addChild(toggler);

    auto lbl = CCLabelBMFont::create("Override Cube Icon", "bigFont.fnt");
    lbl->setColor(ccc3(255, 255, 255));
    lbl->setScale(0.45f);
    lbl->setAnchorPoint(ccp(0, 0.5f));
    lbl->setPosition(centre + ccp(-62, -82));
    this->addChild(lbl);

    return true;
}

void IconNode::onToggleOverride(CCObject* sender)
{
    if (auto m = Module::getByID("cube-icon-override"))
        m->setUserEnabled(!m->getRealEnabled());
}
