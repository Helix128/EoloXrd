#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <Arduino.h>
#include <string.h>
#include "../Scenes/IScene.h"
#include "Profiler.h"

struct Context;

struct SceneEntry
{
    const char *name;
    IScene *scene;
};

template <typename SceneT>
SceneEntry makeScene(SceneT &scene)
{
    return {SceneT::Name, &scene};
}

class SceneManager
{
private:
    static const SceneEntry *sceneEntries;
    static size_t sceneCount;
    static IScene *currentScene;
    static const char *currentSceneName;
    static int currentSceneIndex;
    static bool sceneChanged;
    static unsigned long lastRenderMs;

    static bool parseSceneCmd(const char *cmd, Context &ctx)
    {
        (void)ctx;
        if (strcmp(cmd, "RESET") == 0)
        {
            LOG_LN("Reiniciando EOLO...");
            ESP.restart();
            return false;
        }
        return true;
    }

public:
    static void setScenes(const SceneEntry *entries, size_t count)
    {
        sceneEntries = entries;
        sceneCount = count;
    }

    static void setScene(const char *name, Context &ctx)
    {
        if (!parseSceneCmd(name, ctx))
            return;

        for (size_t i = 0; i < sceneCount; i++)
        {
            if (strcmp(sceneEntries[i].name, name) == 0)
            {
                currentScene = sceneEntries[i].scene;
                currentSceneName = sceneEntries[i].name;
                currentSceneIndex = (int)i;
                sceneChanged = true;
                currentScene->markDirty();
                currentScene->enter(ctx);
                LOG_F("Escena cambiada a: %s (índice %d)\n", name, currentSceneIndex);
                return;
            }
        }
        LOG_F("Escena no encontrada: %s\n", name);
    }

    static void update(Context &ctx, bool externalDirty = false)
    {
        if (currentScene == nullptr)
            return;

        unsigned long now = millis();
        bool sceneDirty = currentScene->isDirty();
        uint16_t interval = currentScene->frameIntervalMs();
        bool intervalElapsed = (uint32_t)(now - lastRenderMs) >= interval;

        if (externalDirty || sceneChanged || sceneDirty || intervalElapsed)
        {
            IScene *renderedScene = currentScene;
            currentScene->update(ctx);
            if (currentScene == renderedScene)
            {
                currentScene->clearDirty();
                sceneChanged = false;
            }
            lastRenderMs = millis();
        }
    }

    static const char *getCurrentSceneName()
    {
        return currentSceneName;
    }

    static IScene *getCurrentScene()
    {
        return currentScene;
    }

    static int getSceneIndex()
    {
        return currentSceneIndex;
    }

    static bool consumeSceneChanged()
    {
        bool changed = sceneChanged;
        sceneChanged = false;
        return changed;
    }
};

inline const SceneEntry *SceneManager::sceneEntries = nullptr;
inline size_t SceneManager::sceneCount = 0;
inline IScene *SceneManager::currentScene = nullptr;
inline const char *SceneManager::currentSceneName = "";
inline int SceneManager::currentSceneIndex = -1;
inline bool SceneManager::sceneChanged = false;
inline unsigned long SceneManager::lastRenderMs = 0;

#endif
