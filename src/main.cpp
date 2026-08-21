// -- EOLO MP --
// Centro de Investigación en Tecnologías para la Sociedad (C+)
// Universidad del Desarrollo
// Diego Muñoz | Helix128
// https://github.com/Helix128/EoloXrd

#include "Application/ActiveApplication.h"

ActiveApplication activeApplication;

void setup()
{
    activeApplication.begin();
}

void loop()
{
    activeApplication.update();
}
