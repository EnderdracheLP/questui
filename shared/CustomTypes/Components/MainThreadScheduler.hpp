#pragma once
#include <vector>
#include "UnityEngine/MonoBehaviour.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(QuestUI, MainThreadScheduler, UnityEngine::MonoBehaviour,

    private:
        static std::vector<std::function<void()>> scheduledMethods;
        static std::mutex scheduledMethodsMutex;
    public:

    static void Schedule(std::function<void()> method) {
        std::lock_guard<std::mutex> lock(scheduledMethodsMutex);
        scheduledMethods.push_back(method);
    }
    
    DECLARE_INSTANCE_METHOD(void, Update);
    
)