#ifndef CAPTURA_BOMBAS_SCENE_H
#define CAPTURA_BOMBAS_SCENE_H

#include "../Setup/ComponentTestScenes.h"

// Entrada desde el menú de captura a la prueba de motores compartida.
class CapturaBombasScene : public ComponentMotorsScene
{
public:
    static constexpr const char *Name = "captura_bombas";

    CapturaBombasScene() : ComponentMotorsScene(ReturnTarget::Capture) {}
};

#endif
