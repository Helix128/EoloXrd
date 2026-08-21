#ifndef EOLO_APPLICATION_UI_APPLICATION_H
#define EOLO_APPLICATION_UI_APPLICATION_H

#ifndef FEATURE_HEADLESS

#include "../Config/Legacy.h"

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#ifdef FEATURE_MODEM
#include "../Board/Modem.h"
#endif
#include "../Board/I2CBus.h"
#include "../Board/SPIBus.h"
#include "../Data/Context.h"
#include "../Drawing/SceneManager.h"
#include "../Drawing/SceneRegistry.h"
#include "../Utility/DebugConsole.h"
#include "../Utility/RS485Monitor.h"
#include "Profiler.h"

// Application shell shared by the interactive EOLO models.  It intentionally
// owns the display before Context so Context continues to receive a fully
// constructed display reference, just as it did when these objects lived in
// main.cpp.
class UiApplication
{
public:
    UiApplication()
#if EOLO_DISPLAY_SPI && EOLO_DISPLAY_HW_SPI
        : _display(U8G2_R0,
                   EoloConfig::displayCsPin,
                   EoloConfig::displayDcPin,
                   EoloConfig::displayResetPin),
          _context(_display)
#elif EOLO_DISPLAY_SPI
        : _display(U8G2_R0,
                   EoloConfig::displayClockPin,
                   EoloConfig::displayMosiPin,
                   EoloConfig::displayCsPin,
                   EoloConfig::displayDcPin,
                   EoloConfig::displayResetPin),
          _context(_display)
#else
        : _display(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN),
          _context(_display)
#endif
    {
        activeInstance_ = this;
    }

    void begin()
    {
#if PPH_PWR_PIN >= 0
        pinMode(PPH_PWR_PIN, OUTPUT); // perifericos
        digitalWrite(PPH_PWR_PIN, HIGH); // Encender perifericos (I2C, display) lo antes posible
#endif
        // El gestor de energia y el ProMini del Eolo MP necesitan varios segundos
        // para estabilizarse tras el N-FET. El worker I2C cuenta desde este punto y
        // mantiene libre el loop/UI durante el warmup.
        I2CBus::getInstance().setWarmupFromNow(I2C_WARMUP_MS);
#ifdef FEATURE_MODEM
        if constexpr (EoloConfig::modemPowerMode == EoloConfig::ModemPowerMode::AlwaysOn)
            Modem::configurePowerPinOn();
        else
            Modem::configurePowerPinOff();
#else
#if MODEM_PWR_PIN >= 0
        pinMode(MODEM_PWR_PIN, OUTPUT); // modem
        digitalWrite(MODEM_PWR_PIN, LOW);
#endif
#endif

        _context.components.motor.begin(); // apagar motores

        Serial.begin(115200);

#ifdef FEATURE_MODEM
        _debugConsole.attachModemService(&_context.components.modemService);
        _debugConsole.attachSensorApi(&_context.components.api);
#endif
        _debugConsole.attachRTC(&_context.components.rtc);
        _debugConsole.attachDisplayReinit(reinitDisplayFromConsole);
        RS485Monitor::getInstance(); // Inicializar monitor RS485
        LOG_LN("RS485 Monitor inicializado");

        // Registrar todas las escenas (SceneRegistry)
        registerAllScenes();

        // Inicialización del contexto de la app
        _context.begin();

        // Carga la escena inicial (splash)
        SceneManager::setScene("splash", _context);
    }

    void update()
    {
        _debugConsole.poll();

        if (SceneManager::getSceneIndex() <= 1)
        {
            _context.components.motor.setPowerPct(0);
        }

        if (millis() - _lastFrameMs < kTargetMs)
        {
            return;
        }

        _frameStartMs = millis();
        _lastFrameMs += kTargetMs;

        // Actualizar el contexto de la app y la escena actual
        const bool externalDirty = _context.update();
        {
            // El OLED actual usa SPI; las lecturas I2C viven en EoloI2CTask y el
            // render no debe esperar al bus.
            // La SD comparte el bus SPI con el OLED. Durante una operación larga de
            // SD se omite este frame para que el loop siga atendiendo sensores,
            // entradas y tareas de fondo en vez de quedar bloqueado en el mutex.
            SPIBus::Guard spiGuard(pdMS_TO_TICKS(1));
            if (spiGuard.acquired())
            {
                SceneManager::update(_context, externalDirty);
            }
        }

        const unsigned long frameExecutionMs = millis() - _frameStartMs;
        PROFILE_MARK("loop.frame", frameExecutionMs * 1000UL);
        RS485Monitor::getInstance().recordLoopFrameTime(frameExecutionMs);
        RS485Monitor::getInstance().checkAndReportViolations();
    }

private:
    static constexpr unsigned long kTargetMs = 8UL;

    // Keep this declaration order: display must outlive Context, and the
    // console is constructed after the application collaborators it attaches.
    DisplayModel _display;
    Context _context;
    DebugConsole _debugConsole;
    unsigned long _lastFrameMs = 0;
    unsigned long _frameStartMs = 0;

    inline static UiApplication *activeInstance_ = nullptr;

    static void reinitDisplayFromConsole()
    {
        if (activeInstance_ != nullptr)
        {
            activeInstance_->reinitDisplay();
        }
    }

    void reinitDisplay()
    {
#if EOLO_DISPLAY_SPI && EOLO_DISPLAY_HW_SPI
        SPIBus::Guard spiGuard;
        SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
        _display.begin();
        _display.setBusClock(EoloConfig::displaySpiClockHz);
#else
        _display.begin();
#if !EOLO_DISPLAY_SPI
        _display.setBusClock(I2C_CLOCK);
#endif
#endif
    }
};

#else
#error "UiApplication requiere un target con interfaz de usuario"
#endif

#endif
