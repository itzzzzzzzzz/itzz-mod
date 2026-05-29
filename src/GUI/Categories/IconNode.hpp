#pragma once

#include "../../Client/CategoryNode.hpp"

using namespace geode::prelude;

// itzz menu: eigener "Icon" Tab (Beta) mit rotierender Icon-Vorschau
class IconNode : public CategoryNode
{
    public:
        CREATE_FUNC(IconNode)
        bool init();
        void onToggleOverride(cocos2d::CCObject* sender);
};

SUBMIT_CATEGORY("Icon", IconNode)
