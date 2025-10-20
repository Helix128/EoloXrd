#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <map>
#include <string>
#include "../Scenes/IScene.h"

// Declaración adelantada de Context
struct Context;

// Clase para manejar escenas
class SceneManager
{
private:
    static std::map<std::string, IScene *> scenes;
    static IScene *currentScene;
    static std::string currentSceneName;

public:
    static void addScene(const std::string &name, IScene *scene)
    {
        scenes[name] = scene;
    }

    static void setScene(const std::string &name, Context &ctx)
    {
        auto it = scenes.find(name);
        if (it != scenes.end())
        {
            currentScene = it->second;
            currentSceneName = name;
            currentScene->enter(ctx); // Llamar método enter de la nueva escena
        }
        else
        {
            Serial.print("Escena no encontrada: ");
            Serial.println(name.c_str());
        }
    }

    static void update(Context &ctx)
    {
        if (currentScene != nullptr)
        {
            currentScene->update(ctx);
        }
    }

    static const std::string &getCurrentSceneName()
    {
        return currentSceneName;
    }
};

// Definiciones de miembros
// inline para que funcionen con static
inline std::map<std::string, IScene *> SceneManager::scenes;
inline IScene *SceneManager::currentScene = nullptr;
inline std::string SceneManager::currentSceneName = "";

#endif