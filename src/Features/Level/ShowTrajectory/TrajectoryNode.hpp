#pragma once

#include <Geode/Geode.hpp>
#include "../Hitboxes/HitboxNode.hpp"

namespace qolmod
{
    class TrajectoryNode : public HitboxNode
    {
        protected:
            static inline TrajectoryNode* instance = nullptr;
            PlayerObject* player = nullptr;
            bool simulating = false;
            float deltaIter = 0.5f;
            int iterCount = 240;
            bool ringSimulated = false;
            std::vector<GameObject*> simulatedRings = {};
            bool existingSimulationCancelled = false;
            geode::Ref<cocos2d::CCNode> trueCheck = nullptr;

            ~TrajectoryNode();
            
            virtual bool shouldDrawTrail();

        public:
            CREATE_FUNC(TrajectoryNode);
            static TrajectoryNode* get();

            bool isSimulating();
            void simulate(PlayerObject* plr, bool held);
            // itzz Autoplay: simuliert plr 'steps' Schritte mit gehaltenem/losgelassenem Sprung
            // und liefert, wie viele Schritte ueberlebt werden (steps = ganzer Horizont ueberlebt).
            int simulateSurvival(PlayerObject* plr, bool held, int steps = 0);
            void simulateFromRing(PlayerObject* player, RingObject* ring);
            void performSimulation(cocos2d::ccColor4F colour, bool useTrail, bool isOrb);

            float getDeltaIter();
            int getIterCount();

            void setDeltaIter(float delta);
            void setIterCount(int iters);

            virtual void redraw();
            virtual bool init();
    };
}