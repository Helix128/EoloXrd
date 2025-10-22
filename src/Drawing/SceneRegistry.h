#ifndef SCENE_REGISTRY_H
#define SCENE_REGISTRY_H

#include "SceneManager.h"
#include "../Scenes/LogoScene.h"
#include "../Scenes/InicioScene.h"
#include "../Scenes/TimeScene.h"
#include "../Scenes/FlujoScene.h"
#include "../Scenes/PlantowerScene.h"
#include "../Scenes/WaitScene.h"


// Función para registrar todas las escenas con el SceneManager
void registerAllScenes()
{ 
    SceneManager::addScene("splash", new LogoScene());
    SceneManager::addScene("inicio", new InicioScene());
    SceneManager::addScene("flujo", new FlujoScene());
    SceneManager::addScene("tiempo", new TimeScene());
    SceneManager::addScene("plantower", new PlantowerScene());
    SceneManager::addScene("wait", new WaitScene());
    Serial.println("Todas las escenas registradas");
}

#endif