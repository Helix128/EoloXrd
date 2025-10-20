#ifndef SCENE_REGISTRY_H
#define SCENE_REGISTRY_H

#include "SceneManager.h"
#include "../Scenes/LogoScene.h"
#include "../Scenes/InicioScene.h"


// Función para registrar todas las escenas con el SceneManager
void registerAllScenes()
{ 
    SceneManager::addScene("splash", new LogoScene());
    SceneManager::addScene("inicio", new InicioScene());
    Serial.println("Todas las escenas registradas");
}

#endif