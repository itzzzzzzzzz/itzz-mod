#include "BackgroundSprite.hpp"
#include "../Utils/Casts.hpp"
#include "Modules/GradientBGColours.hpp"
#include <ThemeManager.hpp>

using namespace qolmod;

bool BackgroundSprite::init()
{
    if (!CCNode::init())
        return false;

    this->scheduleUpdate();

    // yeezy: helles Panel (panel_frame) mit runden Ecken + Hairline
    colouredBG = CCScale9Sprite::createWithSpriteFrameName("yz_panel_frame.png"_spr);
    usingYzPanel = (colouredBG != nullptr);
    if (!colouredBG)
        colouredBG = CCScale9Sprite::create("square.png");
    if (usingYzPanel)
        colouredBG->setCapInsets(CCRectMake(20, 20, 600 - 40, 330 - 40));
    outlineSpr = CCScale9Sprite::create("square.png");

    gradientBG = CCLayerGradient::create(ccc4(0, 0, 0, 255), ccc4(0, 0, 0, 255));
    gradientBG->ignoreAnchorPointForPosition(false);
    gradientBG->setVector(ccp(0, -1));
    gradientBG->setScaleX(-1);
    gradientBG->setScaleY(-1);

    clippingStencil = CCScale9Sprite::create("GJ_square01.png");
    clippingStencil2 = CCScale9Sprite::create("GJ_square01.png");
    clipping = CCClippingNode::create(clippingStencil);
    clipping->setAlphaThreshold(0.03f);

    gradientOutline = CCScale9Sprite::create("GJ_square07.png");
    gradientDarken = CCScale9Sprite::create("GJ_square01.png");

    gradientDarken->setColor(ccc3(0, 0, 0));
    gradientDarken->setOpacity(30);
    gradientDarken->setScale(0.5f);

    clipping->addChild(gradientBG);
    clipping->addChild(gradientDarken);

    clippingCustom = CCClippingNode::create(clippingStencil2);
    clippingCustom->setAlphaThreshold(0.03f);

    clippingCustom->addChild(gradientDarken);

    setTheme((int)ThemeManager::get()->getBackground());

    this->addChildAtPosition(colouredBG, Anchor::Center);
    this->addChildAtPosition(clipping, Anchor::Center);
    this->addChildAtPosition(clippingCustom, Anchor::Center);
    this->addChildAtPosition(outlineSpr, Anchor::Center);
    this->addChildAtPosition(gradientOutline, Anchor::Center);
    return true;
}

void BackgroundSprite::setTheme(int theme2)
{
    this->theme = (qolmod::BackgroundType)theme2;

    // yeezy: helles off-white Panel
    colouredBG->setColor(ccc3(250, 250, 248));
    colouredBG->setOpacity(255);
    colouredBG->setVisible(true);

    outlineSpr->setVisible(false);

    // alte Gradient-/Custom-/Darken-Layer komplett aus
    clipping->setVisible(false);
    clipping->getStencil()->setVisible(false);
    gradientOutline->setVisible(false);
    gradientDarken->setVisible(false);
    clippingCustom->setVisible(false);
}

void BackgroundSprite::setGradientDarkenVisible(bool visible)
{
    gradientDarkenVisible = visible;

    gradientDarken->setVisible((theme == BackgroundType::Gradient || theme == BackgroundType::Custom) && gradientDarkenVisible);
}

void BackgroundSprite::setContentSize(const CCSize& contentSize)
{
    CCNode::setContentSize(contentSize);

    colouredBG->setContentSize(contentSize);
    outlineSpr->setContentSize(contentSize);

    if (itzzBg && itzzBg->getContentWidth() > 0) // Chrome-Bild auf Menuegroesse strecken
    {
        itzzBg->setScaleX(contentSize.width / itzzBg->getContentWidth());
        itzzBg->setScaleY(contentSize.height / itzzBg->getContentHeight());
    }

    clippingStencil->setContentSize(contentSize - ccp(2, 2));
    clippingStencil2->setContentSize(contentSize - ccp(2, 2));
    gradientBG->setContentSize(contentSize);
    gradientOutline->setContentSize(contentSize);
    gradientDarken->setContentSize((contentSize - ccp(15, 15)) / 0.5f);
    if (customImg) // itzz menu: customImg wird nicht mehr erstellt -> null-sicher
    {
        customImg->setScaleX(getContentWidth() / customImg->getContentWidth());
        customImg->setScaleY(getContentHeight() / customImg->getContentHeight());
    }
}

void BackgroundSprite::update(float dt)
{
    gradientBG->setStartColor(GradientBGStart::get()->getColour());
    gradientBG->setEndColor(GradientBGEnd::get()->getColour());
    gradientBG->setVector(GradientBGDirection::get()->getRotationVector());
}

void BackgroundSprite::setOpacity(float opacity)
{
    colouredBG->setOpacity((theme == BackgroundType::Darken ? 175.0f : 255.0f) * (opacity / 255.0f));
    outlineSpr->setOpacity(opacity);
    gradientBG->setOpacity(opacity);
    gradientOutline->setOpacity(opacity);
    gradientDarken->setOpacity(opacity);
}

void BackgroundSprite::setColour(ccColor3B colour)
{
    colouredBG->setColor(theme == BackgroundType::Darken ? ccc3(0, 0, 0) : colour);
    outlineSpr->setColor(colour);
    gradientOutline->setColor(colour);
}

void BackgroundSprite::updateCustomSprite()
{
    if (customImg)
        customImg->removeFromParent();
    
    customImg = ThemeManager::get()->createCustomSprite();

    clippingCustom->addChild(customImg);
    customImg->setScaleX(getContentWidth() / customImg->getContentWidth());
    customImg->setScaleY(getContentHeight() / customImg->getContentHeight());
}