#ifndef SCENE_REGISTRY_H
#define SCENE_REGISTRY_H

#include "SceneManager.h"
#include "../Scenes/LogoScene.h"
#include "../Scenes/InicioScene.h"
#include "../Scenes/Setup/TimeScene.h"
#include "../Scenes/Setup/FlujoScene.h"
#include "../Scenes/Setup/InstantFlujoScene.h"
#include "../Scenes/Setup/PlantowerScene.h"
#include "../Scenes/Captura/WaitScene.h"
#include "../Scenes/Captura/CapturaScene.h"
#include "../Scenes/Captura/FinScene.h"
#include "../Scenes/Captura/MenuFinScene.h"


// Función para registrar todas las escenas con el SceneManager
void registerAllScenes()
{ 
    SceneManager::addScene("splash", new LogoScene());
    SceneManager::addScene("inicio", new InicioScene());
    SceneManager::addScene("flujo", new FlujoScene());
    SceneManager::addScene("flujo_now", new InstantFlujoScene());
    SceneManager::addScene("tiempo", new TimeScene());
    SceneManager::addScene("plantower", new PlantowerScene());
    SceneManager::addScene("wait", new WaitScene());
    SceneManager::addScene("captura", new CapturaScene());
    SceneManager::addScene("end", new FinScene());
    SceneManager::addScene("end_menu", new EndMenuScene());
    
    Serial.println("Todas las escenas registradas");
}

#endif