#ifndef SCENE_REGISTRY_H
#define SCENE_REGISTRY_H

#include "SceneManager.h"
#include "../Scenes/LogoScene.h"
#include "../Scenes/InicioScene.h"
#include "../Scenes/Setup/CalibrationScene.h"
#include "../Scenes/Setup/TimeScene.h"
#include "../Scenes/Setup/FlujoScene.h"
#include "../Scenes/Setup/InstantFlujoScene.h"
#include "../Scenes/Setup/PlantowerScene.h"
#include "../Scenes/Captura/WaitScene.h"
#include "../Scenes/Captura/CapturaScene.h"
#include "../Scenes/Captura/MenuCapturaScene.h"
#include "../Scenes/Captura/CapturaFlujoScene.h"
#include "../Scenes/Captura/HoraFinScene.h"
#include "../Scenes/Captura/FinScene.h"
#include "../Scenes/Captura/MenuFinScene.h"
#include "../Scenes/Captura/CapturaBombasScene.h"
#include "../Scenes/Captura/PlantowerFinScene.h"



// Función para registrar todas las escenas con el SceneManager
void registerAllScenes()
{
    static LogoScene         s_splash;
    static InicioScene       s_inicio;
    static CalibrationScene  s_calibration;
    static FlujoScene        s_flujo;
    static InstantFlujoScene s_flujo_now;
    static TimeScene         s_tiempo;
    static PlantowerScene    s_plantower;
    static WaitScene         s_wait;
    static PlantowerFinScene s_plantower_fin;
    static CapturaScene      s_captura;
    static MenuCapturaScene  s_captura_menu;
    static CapturaFlujoScene s_captura_flujo;
    static TimeEndScene      s_time_end;
    static FinScene          s_end;
    static EndMenuScene      s_end_menu;
    static CapturaBombasScene s_captura_bombas;

    SceneManager::addScene("splash",        &s_splash);
    SceneManager::addScene("inicio",        &s_inicio);
    SceneManager::addScene("calibration",   &s_calibration);
    SceneManager::addScene("flujo",         &s_flujo);
    SceneManager::addScene("flujo_now",     &s_flujo_now);
    SceneManager::addScene("tiempo",        &s_tiempo);
    SceneManager::addScene("plantower",     &s_plantower);
    SceneManager::addScene("wait",          &s_wait);
    SceneManager::addScene("plantower_fin", &s_plantower_fin);
    SceneManager::addScene("captura",       &s_captura);
    SceneManager::addScene("captura_menu",  &s_captura_menu);
    SceneManager::addScene("captura_flujo", &s_captura_flujo);
    SceneManager::addScene("time_end",      &s_time_end);
    SceneManager::addScene("end",           &s_end);
    SceneManager::addScene("end_menu",      &s_end_menu);
    SceneManager::addScene("captura_bombas",&s_captura_bombas);
    LOG_LN("Todas las escenas registradas");
}

#endif