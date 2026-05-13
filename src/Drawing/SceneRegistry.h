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

namespace SceneName
{
    constexpr const char *Splash = LogoScene::Name;
    constexpr const char *Inicio = InicioScene::Name;
    constexpr const char *Calibration = CalibrationScene::Name;
    constexpr const char *Flujo = FlujoScene::Name;
    constexpr const char *FlujoNow = InstantFlujoScene::Name;
    constexpr const char *Tiempo = TimeScene::Name;
    constexpr const char *Plantower = PlantowerScene::Name;
    constexpr const char *Wait = WaitScene::Name;
    constexpr const char *PlantowerFin = PlantowerFinScene::Name;
    constexpr const char *Captura = CapturaScene::Name;
    constexpr const char *CapturaMenu = MenuCapturaScene::Name;
    constexpr const char *CapturaFlujo = CapturaFlujoScene::Name;
    constexpr const char *TimeEnd = TimeEndScene::Name;
    constexpr const char *End = FinScene::Name;
    constexpr const char *EndMenu = EndMenuScene::Name;
    constexpr const char *CapturaBombas = CapturaBombasScene::Name;
}

inline void registerAllScenes()
{
    static LogoScene s_splash;
    static InicioScene s_inicio;
    static CalibrationScene s_calibration;
    static FlujoScene s_flujo;
    static InstantFlujoScene s_flujo_now;
    static TimeScene s_tiempo;
    static PlantowerScene s_plantower;
    static WaitScene s_wait;
    static PlantowerFinScene s_plantower_fin;
    static CapturaScene s_captura;
    static MenuCapturaScene s_captura_menu;
    static CapturaFlujoScene s_captura_flujo;
    static TimeEndScene s_time_end;
    static FinScene s_end;
    static EndMenuScene s_end_menu;
    static CapturaBombasScene s_captura_bombas;

    static const SceneEntry scenes[] = {
        makeScene(s_splash),
        makeScene(s_inicio),
        makeScene(s_calibration),
        makeScene(s_flujo),
        makeScene(s_flujo_now),
        makeScene(s_tiempo),
        makeScene(s_plantower),
        makeScene(s_wait),
        makeScene(s_plantower_fin),
        makeScene(s_captura),
        makeScene(s_captura_menu),
        makeScene(s_captura_flujo),
        makeScene(s_time_end),
        makeScene(s_end),
        makeScene(s_end_menu),
        makeScene(s_captura_bombas),
    };

    SceneManager::setScenes(scenes, sizeof(scenes) / sizeof(scenes[0]));
    LOG_LN("Todas las escenas registradas");
}

#endif
