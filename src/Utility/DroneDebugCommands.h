#ifndef DRONE_DEBUG_COMMANDS_H
#define DRONE_DEBUG_COMMANDS_H

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "../Board/CaptureSwitches.h"
#include "../Data/Context.h"

class DroneDebugCommands : public ConsoleCommandHandler
{
private:
  Context *_ctx = nullptr;
  CaptureSwitches *_switches = nullptr;

  static String formatDuration(uint32_t seconds)
  {
    if (seconds == DRONE_DURATION_INFINITE)
      return "infinita";

    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    char buffer[32];
    if (hours > 0)
      snprintf(buffer, sizeof(buffer), "%lu h %02lu min", (unsigned long)hours, (unsigned long)minutes);
    else if (minutes > 0)
      snprintf(buffer, sizeof(buffer), "%lu min %02lu s", (unsigned long)minutes, (unsigned long)secs);
    else
      snprintf(buffer, sizeof(buffer), "%lu s", (unsigned long)secs);
    return String(buffer);
  }

  static const char *sdStatusText(SDStatus status)
  {
    switch (status)
    {
    case SD_OK:
      return "ok";
    case SD_WRITING:
      return "escribiendo";
    case SD_MISSING:
      return "SD ausente";
    case SD_ERROR:
    default:
      return "error SD";
    }
  }

#ifdef FEATURE_NEOPIXEL
  static const char *ledText(StatusLedPattern pattern)
  {
    switch (pattern)
    {
    case StatusLedPattern::Off:
      return "apagado - sin indicacion";
    case StatusLedPattern::Boot:
      return "azul pulso - inicio/idle";
    case StatusLedPattern::Setup:
      return "violeta pulso - setup Wi-Fi";
    case StatusLedPattern::Waiting:
      return "ambar pulso - esperando inicio";
    case StatusLedPattern::Capturing:
      return "verde pulso - captura activa";
    case StatusLedPattern::Busy:
      return "cian pulso - escribiendo/log activo";
    case StatusLedPattern::Error:
      return "rojo pulso - error SD";
    case StatusLedPattern::Finished:
      return "blanco fijo - captura finalizada";
    default:
      return "desconocido";
    }
  }

  static const char *ledName(StatusLedPattern pattern)
  {
    switch (pattern)
    {
    case StatusLedPattern::Off:
      return "Off";
    case StatusLedPattern::Boot:
      return "Boot";
    case StatusLedPattern::Setup:
      return "Setup";
    case StatusLedPattern::Waiting:
      return "Waiting";
    case StatusLedPattern::Capturing:
      return "Capturing";
    case StatusLedPattern::Busy:
      return "Busy";
    case StatusLedPattern::Error:
      return "Error";
    case StatusLedPattern::Finished:
      return "Finished";
    default:
      return "?";
    }
  }
#endif

  const char *capturePhaseText(const DateTime &now) const
  {
    if (_ctx->isEnd)
      return "finalizada";
    if (_ctx->isPaused)
      return "pausada";
    if (_ctx->isCapturing)
      return "capturando";
    if (_ctx->session.startDate.unixtime() > now.unixtime())
      return "esperando";
#ifdef FEATURE_NEOPIXEL
    if (_ctx->components.statusLed.pattern() == StatusLedPattern::Setup)
      return "setup Wi-Fi";
#endif
    return "idle";
  }

  String captureLine(const DateTime &now) const
  {
    uint32_t nowTs = now.unixtime();
    uint32_t startTs = _ctx->session.startDate.unixtime();
    String line = "Captura: ";

    if (_ctx->isCapturing)
    {
      line += "transcurrido ";
      line += formatDuration(_ctx->session.elapsedTime);
      line += " | ";
      if (_ctx->session.duration == DRONE_DURATION_INFINITE)
      {
        line += "duracion infinita";
      }
      else
      {
        uint32_t remaining = _ctx->session.elapsedTime >= _ctx->session.duration ? 0 : _ctx->session.duration - _ctx->session.elapsedTime;
        line += "termina en ";
        line += formatDuration(remaining);
      }
    }
    else if (startTs > nowTs)
    {
      line += "inicia en ";
      line += formatDuration(startTs - nowTs);
      line += " | duracion ";
      line += formatDuration(_ctx->session.duration);
    }
    else if (_ctx->isEnd)
    {
      line += "finalizada";
    }
    else
    {
      line += "sin captura activa";
    }

    line += " | flujo objetivo ";
    line += String(_ctx->session.targetFlow, 1);
    line += " L/min";
    return line;
  }

  static void printHelp(Print &out)
  {
    out.println("Comandos Dron:");
    out.println("  drone              alias de drone status");
    out.println("  drone status       estado compacto");
    out.println("  drone status -v    estado con detalles raw");
    out.println("  drone help         ayuda");
  }

  static void printSwitchPins(Print &out, const CaptureSwitchSnapshot &snap)
  {
    out.printf("  pins: WAIT0=%d WAIT1=%d DUR0=%d DUR1=%d\n",
               snap.waitSw0.level,
               snap.waitSw1.level,
               snap.durationSw0.level,
               snap.durationSw1.level);
    out.printf("  codes: wait=%u%u duration=%u%u shouldStart=%s\n",
               (unsigned int)((snap.waitCode >> 1) & 0x01),
               (unsigned int)(snap.waitCode & 0x01),
               (unsigned int)((snap.durationCode >> 1) & 0x01),
               (unsigned int)(snap.durationCode & 0x01),
               snap.selection.shouldStart ? "si" : "no");
  }

public:
  void attachContext(Context *ctx)
  {
    _ctx = ctx;
  }

  void attachCaptureSwitches(CaptureSwitches *switches)
  {
    _switches = switches;
  }

  bool handle(const String &line, Print &out) override
  {
    if (!(line == "drone" || line.startsWith("drone ")))
      return false;

    String args = line == "drone" ? String("status") : line.substring(strlen("drone "));
    args.trim();

    if (args == "help" || args == "?")
    {
      printHelp(out);
      return true;
    }

    bool verbose = false;
    if (args == "status -v" || args == "status verbose" || args == "status --verbose")
    {
      verbose = true;
      args = "status";
    }

    if (args != "status")
    {
      out.println("Comando drone desconocido. Usa: drone help");
      return true;
    }

    if (_ctx == nullptr)
    {
      out.println("Contexto Dron no adjuntado.");
      return true;
    }

    DateTime now = _ctx->components.rtc.now();
    FlowData flowData;
    bool flowOk = _ctx->components.flowSensor.getData(flowData);

    NTCData ntcData;
    bool ntcOk = false;
#ifdef FEATURE_NTC
    ntcOk = _ctx->components.ntc.getData(ntcData);
#endif

    out.println("EOLO Dron");
    out.printf("Estado: %s\n", capturePhaseText(now));
    out.println(captureLine(now));

    if (_switches != nullptr)
      CaptureSwitches::printSummary(out, _switches->snapshot());
    else
      out.println("Switches: no adjuntados");

    out.printf("SD: %s\n", sdStatusText(_ctx->sdStatus));

    out.printf("Sensores: flujo ");
    if (flowOk)
      out.printf("%.2f L/min", flowData.flow);
    else
      out.print("N/D");
    out.print(" | NTC ");
    if (ntcOk)
      out.printf("%.1f C", ntcData.temperature);
    else
      out.print("N/D");
    out.printf(" | BME %.1f C %.1f%% %.1f hPa\n",
               _ctx->components.bme.temperature,
               _ctx->components.bme.humidity,
               _ctx->components.bme.pressure);

    out.printf("Motor: %d%% pwm %d/%d modo %s\n",
               _ctx->components.motor.getPowerPct(),
               _ctx->components.motor.getMotorPwm(0),
               _ctx->components.motor.getMotorTargetPwm(0),
#if MOTOR_FLOW_CONTROL_MODE == MOTOR_FLOW_CONTROL_CLOSED_LOOP
               "closed-loop PI"
#else
               "calibracion fija"
#endif
               );

#ifdef FEATURE_NEOPIXEL
    StatusLedPattern pattern = _ctx->components.statusLed.pattern();
    out.printf("LED: %s\n", ledText(pattern));
#else
    out.println("LED: no compilado");
#endif
    out.printf("RTC: %s\n", now.timestamp().c_str());

    if (verbose)
    {
      out.println("Detalle:");
      out.printf("  flags: active=%s paused=%s end=%s log=%s uploadPending=%s uploadActive=%s\n",
                 _ctx->isCapturing ? "si" : "no",
                 _ctx->isPaused ? "si" : "no",
                 _ctx->isEnd ? "si" : "no",
                 _ctx->logActive ? "si" : "no",
                 _ctx->uploadPending ? "si" : "no",
                 _ctx->uploadActive ? "si" : "no");
      out.printf("  SD: ready=%s enum=%d\n", _ctx->isSdReady ? "si" : "no", (int)_ctx->sdStatus);
      out.printf("  session: start=%s elapsed=%lu duration=%u target=%.2f volume=%.3f L\n",
                 _ctx->session.startDate.timestamp().c_str(),
                 _ctx->session.elapsedTime,
                 _ctx->session.duration,
                 _ctx->session.targetFlow,
                 _ctx->session.capturedVolume);
      if (ntcOk)
        out.printf("  NTC raw=%d\n", ntcData.raw);
#ifdef FEATURE_NEOPIXEL
      out.printf("  LED enum: %s (%u)\n", ledName(pattern), (unsigned int)pattern);
#endif
      if (_switches != nullptr)
        printSwitchPins(out, _switches->snapshot());
    }

    return true;
  }
};

#endif

#endif
