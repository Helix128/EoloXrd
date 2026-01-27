#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <map>
#include <string>
#include "../Scenes/IScene.h"
#include "Profiler.h" 

// Declaración adelantada de Context
struct Context;

// Clase para manejar escenas
class SceneManager
{
private:
    static std::map<std::string, IScene *> scenes;
    static std::map<int, IScene *> sceneIndices;
    static IScene *currentScene;
    static std::string currentSceneName;
    static int currentSceneIndex;
public:
    static void addScene(const std::string &name, IScene *scene)
    {
        scenes[name] = scene;
        sceneIndices[sceneIndices.size()] = scene;
    }

    static bool parseSceneCmd(const std::string &cmd, Context &ctx)
    {
        if(cmd=="RESET"){
            LOG_LN("Reiniciando EOLO...");
            ESP.restart();
            return false;
        } 
        return true;

    }
    static void setScene(const std::string &name, Context &ctx)
    {   
        if(!parseSceneCmd(name, ctx)){
            return;
        }
        
        auto it = scenes.find(name);
        if (it != scenes.end())
        {   
            currentScene = it->second;
            currentSceneName = name;
            currentScene->enter(ctx); // Llamar método enter de la nueva escena
            Serial.print("Cambiando a escena: ");

            currentSceneIndex = 0;
            for(const auto& pair : sceneIndices){
                if(pair.second == currentScene){
                    currentSceneIndex = pair.first;
                    break;
                }
            }

            LOG_F("%s (index %d)\n", name.c_str(), currentSceneIndex);
        }
        else
        {
            Serial.print("Escena no encontrada: ");
            LOG_LN(name.c_str());
        }
    }

    static void update(Context &ctx)
    {   
        //Profiler p("SceneManager update");
        if (currentScene != nullptr)
        {
            currentScene->update(ctx);
        }
    }

    static const std::string &getCurrentSceneName()
    {
        return currentSceneName;
    }

    static IScene* getCurrentScene()
    {
        return currentScene;
    }
    static int getSceneIndex(){
        return currentSceneIndex;
    }
};

// Definiciones de miembros
// inline para que funcionen con static
inline std::map<std::string, IScene *> SceneManager::scenes;
inline std::map<int, IScene *> SceneManager::sceneIndices;
inline IScene *SceneManager::currentScene = nullptr;
inline std::string SceneManager::currentSceneName = "";
inline int SceneManager::currentSceneIndex = -1;

#endif