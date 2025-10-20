#ifndef ISCENE_H
#define ISCENE_H

// Declaración adelantada de Context para evitar dependencias circulares
struct Context;

// Interfaz para definir una escena en la interfaz
class IScene
{
public:
    // Llamado al entrar en la escena (para inicialización única)
    virtual void enter(Context &ctx) {}

    // Llamado en cada ciclo de actualización
    virtual void update(Context &ctx) = 0;

    // Destructor virtual para limpieza adecuada
    virtual ~IScene() {}
};
#endif