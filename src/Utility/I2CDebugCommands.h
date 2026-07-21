#ifndef I2C_DEBUG_COMMANDS_H
#define I2C_DEBUG_COMMANDS_H

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "../Board/I2CBus.h"

class I2CDebugCommands : public ConsoleCommandHandler {
private:
    void (*_displayReinitFn)() = nullptr;

    static void printRelativeTime(Print& out, const char* label,
                                  uint32_t timestampMs, bool known) {
        if (!known) {
            out.printf("%s: sin registro", label);
            return;
        }
        uint32_t now = millis();
        out.printf("%s: hace %lu ms (a los %lu ms desde el arranque)",
                   label,
                   (unsigned long)(now - timestampMs),
                   (unsigned long)timestampMs);
    }

    static void printNextRetry(Print& out,
                               const I2CBus::AddressStats& stats,
                               uint32_t now) {
        out.print("proximo intento: ");
        if (stats.backoffMs == 0) {
            out.print("—");
            return;
        }
        int32_t remainingMs = (int32_t)(stats.nextRetryMs - now);
        if (remainingMs <= 0)
            out.print("ahora");
        else
            out.printf("en %ld ms", (long)remainingMs);
    }

    static void printLastScan(Print& out, I2CBus& bus) {
        if (!bus.hasScanResult()) {
            out.print("ultimo scan: sin resultado");
            return;
        }

        out.printf("ultimo scan: %u dispositivo(s)",
                   (unsigned int)bus.getLastScanCount());
        if (bus.getLastScanCount() != 0) {
            out.print(" [");
            bool first = true;
            for (uint8_t address = 1; address < 127; ++address) {
                if (!bus.lastScanFound(address))
                    continue;
                if (!first)
                    out.print(", ");
                out.printf("0x%02X", address);
                first = false;
            }
            out.print("]");
        }
        uint32_t completedMs = bus.lastScanCompletedMs();
        if (completedMs != 0) {
            out.printf(" (hace %lu ms; a los %lu ms desde el arranque)",
                       (unsigned long)(millis() - completedMs),
                       (unsigned long)completedMs);
        }
    }

    void printHelp(Print& out) {
        out.println("Comandos I2C:");
        out.println("  i2c scan       escanear bus I2C, listar dispositivos");
        out.println("  i2c reset      encolar recuperación del bus I2C");
        out.println("  i2c status     mostrar estadisticas y warmup del bus");
    }

public:
    void attachDisplayReinit(void (*fn)()) {
        _displayReinitFn = fn;
    }

    bool handle(const String& line, Print& out) override {
        if (!(line == "i2c" || line.startsWith("i2c "))) return false;

        String args = line.substring(3);
        args.trim();

        if (args.length() == 0 || args == "help" || args == "?") {
            printHelp(out);
            return true;
        }

        if (args == "scan") {
            I2CBus &bus = I2CBus::getInstance();
            if (bus.scanInProgress() || bus.scanQueued()) {
                out.println("Ya hay un scan I2C activo o en cola; espera su resultado.");
            } else {
                bool queued = bus.enqueueCommand(I2CBus::Command::Scan);
                out.println(queued
                                ? "Scan I2C encolado; mostrará inicio, direcciones y resultado final."
                                : "No se pudo encolar el scan I2C (cola llena).");
            }
        } else if (args == "status") {
            I2CBus &bus = I2CBus::getInstance();
            I2CBus::Stats stats = bus.getStats();
            out.printf("Bus=%s warmup=%s restante=%lu ms\n",
                        bus.isReady() ? "OK" : "NO",
                        bus.warmupComplete() ? "completo" : "activo",
                        (unsigned long)bus.warmupRemainingMs());
            out.printf("tx=%lu ok=%lu fallos=%lu nack=%lu timeout=%lu short=%lu recoveries=%lu\n",
                        (unsigned long)stats.transactions,
                        (unsigned long)stats.successes,
                        (unsigned long)stats.failures,
                        (unsigned long)stats.nacks,
                        (unsigned long)stats.timeouts,
                        (unsigned long)stats.shortReads,
                        (unsigned long)stats.recoveries);
            out.printf("ultima transaccion: addr=0x%02X resultado=%s duracion=%lu ms max=%lu ms\n",
                        stats.lastAddress,
                        I2CBus::resultName(stats.lastResult),
                        (unsigned long)stats.lastTransactionMs,
                        (unsigned long)stats.maxTransactionMs);
            printRelativeTime(out, "ultimo exito", stats.lastSuccessMs,
                              stats.successes != 0);
            out.println();
            out.printf("cola: pendientes=%lu completados=%lu descartados=%lu | ",
                        (unsigned long)bus.queuedCommands(),
                        (unsigned long)bus.completedCommands(),
                        (unsigned long)bus.queuedCommandDrops());
            if (bus.scanInProgress()) {
                out.printf("scan=activo (proxima direccion=0x%02X)\n",
                           bus.scanProgressAddress());
            } else if (bus.scanQueued()) {
                out.println("scan=en cola");
            } else {
                out.println("scan=inactivo");
            }
            printLastScan(out, bus);
            out.println();
            const uint8_t addresses[] = {ATTINY_ADDRESS, 0x0A, 0x68, 0x76, 0x77};
            uint32_t now = millis();
            for (uint8_t address : addresses) {
                I2CBus::AddressStats addressStats = bus.getAddressStats(address);
                out.printf("  0x%02X: ", address);
                if (!addressStats.observed) {
                    out.println("SIN_CONSULTAS");
                    continue;
                }
                out.printf("estado=%s fallos=%u backoff=%lu ms | ",
                            I2CBus::resultName(addressStats.lastResult),
                            addressStats.consecutiveFailures,
                            (unsigned long)addressStats.backoffMs);
                printNextRetry(out, addressStats, now);
                if (addressStats.lastSuccessMs != 0) {
                    out.print(" | ");
                    printRelativeTime(out, "ultimo exito",
                                      addressStats.lastSuccessMs, true);
                }
                if (addressStats.lastFailureMs != 0) {
                    out.print(" | ");
                    printRelativeTime(out, "ultimo fallo",
                                      addressStats.lastFailureMs, true);
                }
                out.println();
            }
        } else if (args == "reset") {
            bool queued = I2CBus::getInstance().enqueueCommand(I2CBus::Command::Reset);
            out.println(queued
                            ? "Reset I2C encolado; el worker lo ejecutará sin bloquear la UI."
                            : "No se pudo encolar el reset I2C (cola llena).");
            if (_displayReinitFn != nullptr)
                out.println("La pantalla SPI no requiere reinicialización por reset I2C.");
        } else {
            out.println("Comando i2c desconocido. Usa: i2c help");
        }

        return true;
    }
};

#endif
