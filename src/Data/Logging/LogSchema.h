#ifndef EOLO_LOG_SCHEMA_H
#define EOLO_LOG_SCHEMA_H

#include <Arduino.h>
#include <SD.h>
#include <RTClib.h>
#include <math.h>
#include <Eolo/Types/AnemometerData.h>
#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/LogTypes.h>
#include <Eolo/Types/NTCData.h>
#include <Eolo/Types/PlantowerData.h>

namespace LogSchema
{
    static constexpr float LogMissingValue = -1.0f;

    inline String header(bool includeState, bool includePlantower,
                         bool includeAnemometer, bool includeNtc);

    template <typename ContextT>
    inline bool plantowerEnabled(const ContextT &ctx)
    {
#ifdef FEATURE_PLANTOWER
        return ctx.session.usePlantower;
#else
        (void)ctx;
        return false;
#endif
    }

    template <typename ContextT>
    inline bool anemometerEnabled(const ContextT &ctx)
    {
#ifdef FEATURE_ANEMOMETER
        (void)ctx;
        return true;
#else
        (void)ctx;
        return false;
#endif
    }

    template <typename ContextT>
    inline bool ntcEnabled(const ContextT &ctx)
    {
#ifdef FEATURE_NTC
        (void)ctx;
        return true;
#else
        (void)ctx;
        return false;
#endif
    }

    template <typename ContextT>
    inline bool stateColumnEnabled(const ContextT &ctx)
    {
#ifdef FEATURE_FLOW_PID
        (void)ctx;
        return true;
#else
        (void)ctx;
        return false;
#endif
    }

    template <typename ContextT>
    inline const char *captureState(ContextT &ctx)
    {
#ifdef FEATURE_FLOW_PID
        if (ctx.motorCapture.getPidStatus().kickActive)
            return "Arrancando";
#endif
        (void)ctx;
        return "Capturando";
    }

    template <typename ContextT>
    inline String header(const ContextT &ctx)
    {
        return header(stateColumnEnabled(ctx), plantowerEnabled(ctx),
                      anemometerEnabled(ctx), ntcEnabled(ctx));
    }

    inline String header(bool includeState, bool includePlantower,
                         bool includeAnemometer, bool includeNtc)
    {
        String out = "time";
        if (includeState)
            out += ",state";
        out += ",flow,flow_target,temperature,humidity,pressure";
        if (includePlantower)
            out += ",pm1,pm25,pm10";
        if (includeAnemometer)
            out += ",wind_speed,wind_direction";
        if (includeNtc)
            out += ",ntc_temperature";
        out += ",battery_pct";
        return out;
    }

    inline float missingIfInvalid(bool valid, float value)
    {
        return valid && isfinite(value) ? value : LogMissingValue;
    }

    inline float bmeValue(bool bmeReady, float value)
    {
        return bmeReady && isfinite(value) && value > -999.0f ? value : LogMissingValue;
    }

    inline void printValue(File &file, float value)
    {
        file.print(value);
    }

    inline void printValue(File &file, int value)
    {
        file.print(value);
    }

    inline void printValue(File &file, const char *value)
    {
        file.print(value);
    }

    inline void printComma(File &file)
    {
        file.print(",");
    }

    // Serializador desacoplado de Context. El wrapper histórico de abajo se
    // mantiene para escenas/tests que todavía consultan sensores en el acto.
    inline void writeRow(File &file, const LogRecord &record,
                         bool includeState, bool includePlantower,
                         bool includeAnemometer, bool includeNtc,
                         const char *stateText = "Capturando")
    {
        file.print(DateTime(record.timestampUnix).timestamp());
        printComma(file);
        if (includeState)
        {
            printValue(file, stateText == nullptr ? "Capturando" : stateText);
            printComma(file);
        }

        printValue(file, missingIfInvalid(record.flow.valid, record.flow.flow));
        printComma(file);
        printValue(file, record.targetFlow);
        printComma(file);
        printValue(file, bmeValue(record.environment.valid, record.environment.temperature));
        printComma(file);
        printValue(file, bmeValue(record.environment.valid, record.environment.humidity));
        printComma(file);
        printValue(file, bmeValue(record.environment.valid, record.environment.pressure));

        if (includePlantower)
        {
            printComma(file);
            printValue(file, missingIfInvalid(record.plantower.valid, static_cast<float>(record.plantower.pm1_0)));
            printComma(file);
            printValue(file, missingIfInvalid(record.plantower.valid, static_cast<float>(record.plantower.pm2_5)));
            printComma(file);
            printValue(file, missingIfInvalid(record.plantower.valid, static_cast<float>(record.plantower.pm10_0)));
        }
        if (includeAnemometer)
        {
            printComma(file);
            printValue(file, missingIfInvalid(record.anemometer.valid, record.anemometer.speed));
            printComma(file);
            printValue(file, record.anemometer.valid ? record.anemometer.direction : static_cast<int>(LogMissingValue));
        }
        if (includeNtc)
        {
            printComma(file);
            printValue(file, missingIfInvalid(record.ntc.valid, record.ntc.temperature));
        }

        printComma(file);
        printValue(file, record.batteryPercent);
        file.println();
    }

    template <typename ContextT>
    inline void writeRow(File &file, ContextT &ctx)
    {
        file.print(ctx.components.rtc.now().timestamp());
        printComma(file);

        if (stateColumnEnabled(ctx))
        {
            printValue(file, captureState(ctx));
            printComma(file);
        }

        FlowData flowData;
        bool flowValid = ctx.components.flowSensor.getData(flowData) && flowData.valid;
        printValue(file, missingIfInvalid(flowValid, flowData.flow));
        printComma(file);

        printValue(file, ctx.session.targetFlow);
        printComma(file);
        BME280Data bmeData;
        bool bmeValid = ctx.components.bme.getData(bmeData);
        printValue(file, bmeValue(bmeValid, bmeData.temperature));
        printComma(file);
        printValue(file, bmeValue(bmeValid, bmeData.humidity));
        printComma(file);
        printValue(file, bmeValue(bmeValid, bmeData.pressure));

#ifdef FEATURE_PLANTOWER
        if (plantowerEnabled(ctx))
        {
            PlantowerData ptowerData;
            bool plantowerValid = ctx.components.plantower.getData(ptowerData) && ptowerData.valid;
            printComma(file);
            printValue(file, missingIfInvalid(plantowerValid, (float)ptowerData.pm1_0));
            printComma(file);
            printValue(file, missingIfInvalid(plantowerValid, (float)ptowerData.pm2_5));
            printComma(file);
            printValue(file, missingIfInvalid(plantowerValid, (float)ptowerData.pm10_0));
        }
#endif

#ifdef FEATURE_ANEMOMETER
        {
            AnemometerData anemoData;
            bool anemometerValid = ctx.components.anemometer.getData(anemoData) && anemoData.valid;
            printComma(file);
            printValue(file, missingIfInvalid(anemometerValid, anemoData.speed));
            printComma(file);
            printValue(file, anemometerValid ? anemoData.direction : (int)LogMissingValue);
        }
#endif

#ifdef FEATURE_NTC
        {
            NTCData ntcData;
            bool ntcValid = ctx.components.ntc.getData(ntcData) && ntcData.valid;
            printComma(file);
            printValue(file, missingIfInvalid(ntcValid, ntcData.temperature));
        }
#endif

        printComma(file);
        printValue(file, ctx.components.battery.getPct());
        file.println();
    }
}

#endif // EOLO_LOG_SCHEMA_H
