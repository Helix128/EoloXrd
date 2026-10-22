#include <unity.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <Eolo/Core/Capture/CaptureControllerModel.h>
#include <Eolo/Core/Capture/CaptureSafetyModel.h>
#include <Eolo/Core/Calibration/MotorCalibrationModel.h>
#include <Eolo/Core/Flow/FlowSchedule.h>
#include <Eolo/Core/Flow/FlowVolumeIntegrator.h>
#include <Eolo/Core/Flow/DualMotorFlowController.h>
#include <Eolo/Core/Input/CaptureSwitchLogic.h>
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Core/Sensors/AnemometerModel.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>
#include <Eolo/Core/Sensors/NtcThermistor.h>
#include <Eolo/Core/Thermal/ThermalProtectionModel.h>
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include <Eolo/Core/Communication/GnssParser.h>
#include <Eolo/Core/Communication/AtResponse.h>
#include <Eolo/Core/Communication/HttpUrl.h>
#include <Eolo/Core/Communication/SignalQuality.h>
#include <Eolo/Core/Ui/StatusLedPatterns.h>
#include <Eolo/Core/Debug/ConsoleArgs.h>
#include <Eolo/Core/Time/RtcTimeParser.h>
#include <Eolo/Types/HeadlessSetupTypes.h>
#include <Eolo/Core/Power/BatteryMath.h>
#include <Eolo/Core/Power/BatteryProtocol.h>
#include <Eolo/Core/Communication/RS485Protocol.h>
#include <Eolo/Core/Communication/RS485Stats.h>
#include <Eolo/Types/ModemHttpContract.h>
#include "../../src/Common/Board/I2CRetryPolicy.h"
#include "../../src/Variants/Profiles/DronLegacy.h"

static void test_battery_2s_pct_follows_discharge_curve()
{
    TEST_ASSERT_EQUAL_FLOAT(100.0f, BatteryMath::pack2sPctFromVoltage(9.0f));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, BatteryMath::pack2sPctFromVoltage(8.40f));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, BatteryMath::pack2sPctFromVoltage(7.68f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, BatteryMath::pack2sPctFromVoltage(6.54f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, BatteryMath::pack2sPctFromVoltage(5.0f));
    // Punto medio entre 7.68 V (50 %) y 7.64 V (45 %).
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 47.5f, BatteryMath::pack2sPctFromVoltage(7.66f));
}

void test_battery_2s_voltage_uses_divider_ratio()
{
    // 8.4 V de pack => 2.625 V en el pin del ADC.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.4f, BatteryMath::pack2sVoltageFromPinMv(2625.0f));
}

void test_rtc_parser_is_calendar_aware()
{
    RtcDateTime value;
    TEST_ASSERT_TRUE(RtcTimeParser::parseDateTime("2028-02-29 23:59:59", value));
    TEST_ASSERT_EQUAL_UINT16(2028, value.year);
    TEST_ASSERT_FALSE(RtcTimeParser::parseDateTime("2026-02-29 12:00:00", value));
    TEST_ASSERT_FALSE(RtcTimeParser::parseDateTime("2026-04-31 12:00:00", value));
}

static void test_session_is_epoch_only()
{
    HeadlessSetupConfig config;
    config.waitSeconds = 300;
    config.durationSeconds = 900;
    config.targetFlow = 4.2f;
    Session session;
    HeadlessSetup::applyToSession(config, session, 1700000000UL);
    TEST_ASSERT_EQUAL_UINT32(1700000300UL, session.startUnix);
    TEST_ASSERT_EQUAL_UINT32(900, session.duration);
}

static void test_flow_schedule_boundary()
{
    FlowSection sections[2] = {{10, 2.0f}, {20, 6.0f}};
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, FlowSchedule::targetAtElapsed(4.0f, 2, sections, 9));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, FlowSchedule::targetAtElapsed(4.0f, 2, sections, 10));
}

static void test_capture_switch_wait_timing_matches_runtime_contract()
{
    CaptureSwitchSelection oneMinute = CaptureSwitchLogic::decode(0b01, 0b01);
    CaptureSwitchSelection fiveMinutes = CaptureSwitchLogic::decode(0b10, 0b01);
    CaptureSwitchSelection instant = CaptureSwitchLogic::decode(0b11, 0b01);

    TEST_ASSERT_EQUAL_UINT32(60UL, oneMinute.waitSeconds);
    TEST_ASSERT_EQUAL_UINT32(5UL * 60UL, fiveMinutes.waitSeconds);
    TEST_ASSERT_EQUAL_UINT32(0UL, instant.waitSeconds);
    TEST_ASSERT_EQUAL_STRING("1 min", CaptureSwitchLogic::waitDescription(0b01));
    TEST_ASSERT_EQUAL_STRING("5 min", CaptureSwitchLogic::waitDescription(0b10));
}

static void test_modem_http_contract_rejects_oversized_url_and_payload()
{
    char maxUrl[ModemHttpContract::kHttpUrlMaxChars + 1] = {};
    memset(maxUrl, 'u', sizeof(maxUrl) - 1);
    TEST_ASSERT_TRUE(ModemHttpContract::urlFits(maxUrl));

    char oversizedUrl[ModemHttpContract::kHttpUrlMaxChars + 2] = {};
    memset(oversizedUrl, 'u', sizeof(oversizedUrl) - 1);
    TEST_ASSERT_FALSE(ModemHttpContract::urlFits(oversizedUrl));

    char maxPayload[ModemHttpContract::kHttpPayloadMaxChars + 1] = {};
    memset(maxPayload, 'p', sizeof(maxPayload) - 1);
    TEST_ASSERT_TRUE(ModemHttpContract::payloadFits(maxPayload));

    char oversizedPayload[ModemHttpContract::kHttpPayloadMaxChars + 2] = {};
    memset(oversizedPayload, 'p', sizeof(oversizedPayload) - 1);
    TEST_ASSERT_FALSE(ModemHttpContract::payloadFits(oversizedPayload));
    TEST_ASSERT_TRUE(ModemHttpContract::payloadFits(nullptr));
    TEST_ASSERT_FALSE(ModemHttpContract::urlFits("https://example.test/\rAT+NETCLOSE"));
    TEST_ASSERT_FALSE(ModemHttpContract::urlFits("https://example.test/\""));
    TEST_ASSERT_FALSE(ModemHttpContract::payloadFits("x\ny"));
}

static void test_plantower_parser_decodes_signed_temperature_and_humidity()
{
    PlantowerParser parser;
    uint8_t frame[32] = {0x42, 0x4D, 0x00, 0x1C};
    // Atmospheric PM readings (buffer offsets 6, 8 and 10).
    frame[10] = 0x00; frame[11] = 0x0A;
    frame[12] = 0x00; frame[13] = 0x19;
    frame[14] = 0x00; frame[15] = 0x64;
    // PMS5003T temperature -2.5 C and humidity 63.4 %.
    frame[26] = 0xFF; frame[27] = 0xE7;
    frame[28] = 0x02; frame[29] = 0x7A;
    uint16_t checksum = 0;
    for (int i = 0; i < 30; ++i) checksum += frame[i];
    frame[30] = checksum >> 8; frame[31] = checksum & 0xFF;
    for (uint8_t byte : frame) parser.processByte(byte);
    PlantowerData data = parser.getData();
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -2.5f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.4f, data.humidity);
}

static void test_gnss_parser_converts_coordinates_speed_and_satellites()
{
    const char *response = "+CGNSSINFO: 1,1,20260731120000.000,3351.1234,S,07038.5678,W,12.0,10.5,0.0,3,,0.9,1.2,0.8,,5,,4,7\r\nOK\r\n";
    GnssData data;
    TEST_ASSERT_TRUE(GnssParser::parse(response, data));
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, -33.852056f, data.latitude);
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, -70.642797f, data.longitude);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 19.446f, data.speedKmh);
    TEST_ASSERT_EQUAL_FLOAT(16.0f, data.satellites);
}

static void test_gnss_parser_rejects_missing_fix()
{
    GnssData data;
    TEST_ASSERT_FALSE(GnssParser::parse("+CGNSSINFO: 1,0,,,,,,,,,,,,,,,,,", data));
    TEST_ASSERT_FALSE(data.valid);
}

static void test_i2c_retry_policy_matches_historical_backoff()
{
    TEST_ASSERT_EQUAL_UINT32(250UL, I2CRetryPolicy::delayForFailures(0));
    TEST_ASSERT_EQUAL_UINT32(250UL, I2CRetryPolicy::delayForFailures(1));
    TEST_ASSERT_EQUAL_UINT32(500UL, I2CRetryPolicy::delayForFailures(2));
    TEST_ASSERT_EQUAL_UINT32(1000UL, I2CRetryPolicy::delayForFailures(3));
    TEST_ASSERT_EQUAL_UINT32(2000UL, I2CRetryPolicy::delayForFailures(4));
    TEST_ASSERT_EQUAL_UINT32(5000UL, I2CRetryPolicy::delayForFailures(5));
    TEST_ASSERT_EQUAL_UINT32(5000UL, I2CRetryPolicy::delayForFailures(255));
}

static void test_fs3000_conversion_boundaries()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, FS3000FlowModel::flowFromVelocity(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, FS3000FlowModel::flowFromVelocity(0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, FS3000FlowModel::flowFromVelocity(1.74f));
    TEST_ASSERT_GREATER_THAN_FLOAT(11.0f, FS3000FlowModel::flowFromVelocity(3.5f));
}

static void test_afm07_fresh_stale_contract()
{
    FlowData data;
    uint32_t lastSuccess = 0;
    AFM07Model::applyReadSuccess(data, lastSuccess, 1234, 1000, 100.0f);
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_TRUE(data.fresh);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.34f, data.flow);
    TEST_ASSERT_TRUE(AFM07Model::refreshAge(data, lastSuccess, 2100, 1200, 15000));
    TEST_ASSERT_TRUE(data.fresh);
    AFM07Model::applyReadFailure(data, lastSuccess, 2150, 1200, 15000);
    TEST_ASSERT_TRUE(data.fresh);
    TEST_ASSERT_FALSE(data.stale);
    AFM07Model::applyReadFailure(data, lastSuccess, 2300, 1200, 15000);
    TEST_ASSERT_FALSE(data.fresh);
    TEST_ASSERT_TRUE(data.stale);
    TEST_ASSERT_TRUE(AFM07Model::refreshAge(data, lastSuccess, 2300, 1200, 15000));
    TEST_ASSERT_TRUE(data.stale);
    TEST_ASSERT_FALSE(AFM07Model::refreshAge(data, lastSuccess, 17001, 1200, 15000));
}

static void test_afm07_sample_at_millis_zero_and_wraparound()
{
    FlowData atBoot;
    uint32_t lastSuccess = 99;
    AFM07Model::applyReadSuccess(atBoot, lastSuccess, 500, 0, 100.0f);
    TEST_ASSERT_EQUAL_UINT32(1, atBoot.sampleId);
    TEST_ASSERT_EQUAL_UINT32(0, lastSuccess);
    TEST_ASSERT_TRUE(AFM07Model::refreshAge(atBoot, lastSuccess, 1000, 1200, 15000));
    TEST_ASSERT_TRUE(atBoot.fresh);

    FlowData wrapped;
    lastSuccess = 0;
    AFM07Model::applyReadSuccess(wrapped, lastSuccess, 500, 0xFFFFFFF0U, 100.0f);
    TEST_ASSERT_TRUE(AFM07Model::refreshAge(wrapped, lastSuccess, 0x00000020U, 1200, 15000));
    TEST_ASSERT_EQUAL_UINT32(0x30U, wrapped.ageMs);
    TEST_ASSERT_TRUE(wrapped.fresh);
}

static void test_flow_volume_integrates_each_physical_sample()
{
    FlowVolumeIntegrator integrator;
    integrator.reset(0);
    float volume = 0.0f;
    for (uint32_t second = 1; second <= 900; ++second)
        volume += integrator.update(second * 1000UL, second, true, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, volume);
    TEST_ASSERT_FLOAT_WITHIN(0.000001f, 0.0f,
                             integrator.update(900000UL, 900, true, 5.0f));

    integrator.reset(0xFFFFFF00U);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f * (512.0f / 60000.0f),
                             integrator.update(0x00000100U, 1, true, 5.0f));
}

static void test_dron_pid_does_not_use_calibration_seed()
{
    TEST_ASSERT_FALSE(EoloConfig::Profile::kUseCalibrationSeed);
}

static void test_anemometer_conversion_and_expiry()
{
    AnemometerData data;
    uint32_t lastSuccess = 0;
    AnemometerModel::applyReadSuccess(data, lastSuccess, 250, 270, 1000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, data.speed);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.0f, data.windKph);
    TEST_ASSERT_EQUAL_INT(270, data.direction);
    TEST_ASSERT_TRUE(AnemometerModel::refreshValidity(data, lastSuccess, 16000, 15000));
    TEST_ASSERT_FALSE(AnemometerModel::refreshValidity(data, lastSuccess, 16001, 15000));
}

static void test_rs485_requests_and_response_validation()
{
    uint8_t request[8] = {};
    TEST_ASSERT_TRUE(EoloCore::ModbusRtuProtocol::buildReadHolding(0x01, 0x0000, 2,
                                                                    request, sizeof(request)));
    const uint8_t expectedAnemRequest[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedAnemRequest, request, sizeof(request));

    TEST_ASSERT_TRUE(EoloCore::ModbusRtuProtocol::buildReadHolding(0x02, 0x0000, 1,
                                                                    request, sizeof(request)));
    const uint8_t expectedAfmRequest[8] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x39};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedAfmRequest, request, sizeof(request));

    uint8_t response[9] = {0x01, 0x03, 0x04, 0x00, 0xFA, 0x01, 0x0E, 0x00, 0x00};
    uint16_t crc = EoloCore::ModbusRtuProtocol::crc16(response, 7);
    response[7] = static_cast<uint8_t>(crc);
    response[8] = static_cast<uint8_t>(crc >> 8);
    uint16_t registers[2] = {};
    EoloCore::ModbusReadResult parsed =
        EoloCore::ModbusRtuProtocol::parseReadResponse(response, sizeof(response), 0x01, 2, registers);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EoloCore::ModbusReadStatus::Ok),
                          static_cast<int>(parsed.status));
    TEST_ASSERT_EQUAL_HEX16(0x00FA, registers[0]);
    TEST_ASSERT_EQUAL_HEX16(0x010E, registers[1]);

    response[8] ^= 0xFF;
    parsed = EoloCore::ModbusRtuProtocol::parseReadResponse(response, sizeof(response), 0x01, 2, registers);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EoloCore::ModbusReadStatus::InvalidCrc),
                          static_cast<int>(parsed.status));

    uint8_t exception[5] = {0x02, 0x83, 0x02, 0x00, 0x00};
    crc = EoloCore::ModbusRtuProtocol::crc16(exception, 3);
    exception[3] = static_cast<uint8_t>(crc);
    exception[4] = static_cast<uint8_t>(crc >> 8);
    parsed = EoloCore::ModbusRtuProtocol::parseReadResponse(exception, sizeof(exception), 0x02, 1, nullptr);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EoloCore::ModbusReadStatus::Exception),
                          static_cast<int>(parsed.status));
    TEST_ASSERT_EQUAL_UINT8(0x02, parsed.exceptionCode);

    // El código 0x04 se conserva byte a byte: el scheduler lo usa para
    // enclavar el sensor y no debe degradarlo a un timeout genérico.
    exception[2] = 0x04;
    crc = EoloCore::ModbusRtuProtocol::crc16(exception, 3);
    exception[3] = static_cast<uint8_t>(crc);
    exception[4] = static_cast<uint8_t>(crc >> 8);
    parsed = EoloCore::ModbusRtuProtocol::parseReadResponse(exception, sizeof(exception), 0x02, 1, nullptr);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EoloCore::ModbusReadStatus::Exception),
                          static_cast<int>(parsed.status));
    TEST_ASSERT_EQUAL_UINT8(0x04, parsed.exceptionCode);
}

static void test_rs485_schedule_reserves_afm07()
{
    using Timing = EoloCore::RS485TimingModel;
    TEST_ASSERT_EQUAL_UINT8(0x02, EoloCore::ModbusRtuProtocol::Afm07SlaveId);
    TEST_ASSERT_EQUAL_UINT32(4800UL, EoloCore::ModbusRtuProtocol::Afm07BaudRate);
    TEST_ASSERT_EQUAL_UINT8(0x03, EoloCore::ModbusRtuProtocol::ReadHoldingRegisters);
    TEST_ASSERT_EQUAL_UINT16(0x0000, EoloCore::ModbusRtuProtocol::Afm07FlowRegister);
    TEST_ASSERT_EQUAL_UINT16(0x0004, EoloCore::ModbusRtuProtocol::Afm07DiagnosticRegister);
    TEST_ASSERT_EQUAL_UINT8(1, EoloCore::ModbusRtuProtocol::Afm07FlowCount);
    TEST_ASSERT_TRUE(Timing::kAnemometerSlotBudgetMs < Timing::kAfmIntervalMs);
    TEST_ASSERT_EQUAL_UINT32(250, Timing::kAfmPollGapMs);
    TEST_ASSERT_EQUAL_UINT32(120, Timing::kAnemometerSlotBudgetMs);
    TEST_ASSERT_EQUAL_UINT32(100, Timing::kMinGapAfterFailureMs);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1199, Timing::kAfmIntervalMs);
    TEST_ASSERT_TRUE(Timing::optionalFitsBeforeCritical(100, 300, true, 120));
    TEST_ASSERT_FALSE(Timing::optionalFitsBeforeCritical(100, 200, true, 120));
    TEST_ASSERT_TRUE(Timing::optionalFitsBeforeCritical(600, 100, true, 120));
    TEST_ASSERT_EQUAL_UINT32(1300, Timing::nextPeriodicDue(1000, 1150, 300));
    TEST_ASSERT_EQUAL_UINT32(0x2CU, Timing::nextPeriodicDue(0xFFFFFF00U, 0xFFFFFFF0U, 100));

    // La siguiente operación nace al terminar la actual; nunca se recuperan
    // vencimientos periódicos acumulados en una ráfaga.
    TEST_ASSERT_EQUAL_UINT32(1475, Timing::nextDueAfterCompletion(1275, 200));
    TEST_ASSERT_EQUAL_UINT32(1475, Timing::nextFailureDue(1000, 1275, 200, false));
    // 0x06 (device busy) añade exactamente una cadencia completa adicional.
    TEST_ASSERT_EQUAL_UINT32(1675, Timing::nextFailureDue(1000, 1275, 200, true));
    // El cálculo sigue siendo modular al cruzar millis() por cero.
    TEST_ASSERT_EQUAL_UINT32(0x00000084U,
                             Timing::nextFailureDue(0xFFFFFF00U, 0x00000020U, 100, false));
    TEST_ASSERT_EQUAL_UINT32(0x000000B8U,
                             Timing::nextDueAfterCompletion(0xFFFFFFF0U, 200));
}

static void test_afm_safety_retries_before_blocking()
{
    using Timing = EoloCore::RS485TimingModel;
    TEST_ASSERT_FALSE(Timing::afmSafetyBlockReached(1));
    TEST_ASSERT_FALSE(Timing::afmSafetyBlockReached(2));
    TEST_ASSERT_TRUE(Timing::afmSafetyBlockReached(Timing::kAfmSafetyFailureAttempts));
    TEST_ASSERT_TRUE(Timing::afmSafetyBlockReached(Timing::kAfmSafetyFailureAttempts + 1));
}

static void test_capture_safety_blocks_missing_prerequisites()
{
    CaptureSafetyInput input;
    input.sdReady = false;
    input.afmValid = true;
    input.afmFresh = true;
    input.ntcValid = true;
    CaptureSafetyOutput start = CaptureSafetyModel::evaluateStart(input);
    TEST_ASSERT_FALSE(start.startAllowed);
    TEST_ASSERT_FALSE(start.motorAllowed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureSafetyFault::SdUnavailable),
                          static_cast<int>(start.fault));

    input.sdReady = true;
    input.afmValid = false;
    start = CaptureSafetyModel::evaluateStart(input);
    TEST_ASSERT_FALSE(start.startAllowed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureSafetyFault::AfmInvalid),
                          static_cast<int>(start.fault));

    input.afmValid = true;
    input.afmFresh = false;
    CaptureSafetyOutput run = CaptureSafetyModel::evaluateRun(input);
    TEST_ASSERT_TRUE(run.abortCapture);
    TEST_ASSERT_FALSE(run.motorAllowed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureSafetyFault::AfmStale),
                          static_cast<int>(run.fault));

    input.afmFresh = true;
    input.afmSafetyBlocked = true;
    run = CaptureSafetyModel::evaluateRun(input);
    TEST_ASSERT_TRUE(run.abortCapture);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureSafetyFault::AfmDiagnostic),
                          static_cast<int>(run.fault));

    input.afmSafetyBlocked = false;
    input.ntcValid = false;
    run = CaptureSafetyModel::evaluateRun(input);
    TEST_ASSERT_TRUE(run.abortCapture);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureSafetyFault::NtcInvalid),
                          static_cast<int>(run.fault));
}

static void test_plantower_frame_and_checksum()
{
    uint8_t frame[32] = {0x42, 0x4d, 0x00, 0x1c};
    frame[10] = 0x00; frame[11] = 0x0a;
    frame[12] = 0x00; frame[13] = 0x19;
    frame[14] = 0x00; frame[15] = 0x64;
    uint16_t checksum = 0;
    for (int i = 0; i < 30; ++i) checksum += frame[i];
    frame[30] = static_cast<uint8_t>(checksum >> 8);
    frame[31] = static_cast<uint8_t>(checksum);

    PlantowerParser parser;
    bool parsed = false;
    for (uint8_t byte : frame) parsed = parser.processByte(byte) || parsed;
    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_UINT16(10, parser.getData().pm1_0);
    TEST_ASSERT_EQUAL_UINT16(25, parser.getData().pm2_5);
    TEST_ASSERT_EQUAL_UINT16(100, parser.getData().pm10_0);

    frame[31] ^= 0xff;
    PlantowerParser invalid;
    parsed = false;
    for (uint8_t byte : frame) parsed = invalid.processByte(byte) || parsed;
    TEST_ASSERT_FALSE(parsed);
}

static void test_capture_state_machine_actions()
{
    CaptureMachineState state;
    state.startUnix = 100;
    state.durationSeconds = 20;

    CaptureMachineInput begin;
    begin.nowUnix = 90;
    begin.begin = true;
    CaptureMachineOutput output = CaptureControllerModel::update(state, begin, 5);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CapturePhase::Waiting), static_cast<int>(state.phase));
    TEST_ASSERT_FALSE(output.startMotor);

    CaptureMachineInput start;
    start.nowUnix = 100;
    output = CaptureControllerModel::update(state, start, 5);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CapturePhase::Running), static_cast<int>(state.phase));
    TEST_ASSERT_TRUE(output.startMotor);

    CaptureMachineInput pause;
    pause.nowUnix = 105;
    pause.pause = true;
    output = CaptureControllerModel::update(state, pause, 5);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CapturePhase::Paused), static_cast<int>(state.phase));
    TEST_ASSERT_TRUE(output.stopMotor);

    CaptureMachineInput resume;
    resume.nowUnix = 110;
    resume.resume = true;
    output = CaptureControllerModel::update(state, resume, 5);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CapturePhase::Running), static_cast<int>(state.phase));
    TEST_ASSERT_TRUE(output.startMotor);
    TEST_ASSERT_EQUAL_UINT32(25, state.durationSeconds);
}

static void test_motor_calibration_model()
{
    MotorCalibrationSampleStats stats;
    stats.add(1.0f);
    stats.add(1.1f);
    stats.add(0.9f);
    MotorCalibrationPoint point;
    TEST_ASSERT_TRUE(MotorCalibrationModel::makePoint(stats, 100, 0.1f, 0.05f, 0.2f, nullptr, point));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, point.flow);
    TEST_ASSERT_FALSE(MotorCalibrationModel::makePoint(stats, 120, 0.1f, 0.05f, 0.01f, nullptr, point));
}

static void test_thermal_protection_dto()
{
    ThermalProtectionInput input;
    input.temperature = 75.0f;
    input.sensorValid = true;
    ThermalProtectionOutput output = ThermalProtectionModel::update(input);
    TEST_ASSERT_TRUE(output.latched);
    TEST_ASSERT_TRUE(output.changed);
    TEST_ASSERT_FALSE(output.motorAllowed);

    input.latched = output.latched;
    input.temperature = 55.0f;
    output = ThermalProtectionModel::update(input);
    TEST_ASSERT_FALSE(output.latched);
    TEST_ASSERT_TRUE(output.changed);
    TEST_ASSERT_TRUE(output.motorAllowed);

    input.sensorValid = false;
    output = ThermalProtectionModel::update(input);
    TEST_ASSERT_FALSE(output.motorAllowed);
}

static void make_battery_frame(uint8_t active, float dc, float batt0,
                               float batt1, uint8_t *raw)
{
    raw[0] = active;
    memcpy(raw + 1, &dc, sizeof(float));
    memcpy(raw + 5, &batt0, sizeof(float));
    memcpy(raw + 9, &batt1, sizeof(float));
    raw[13] = active;
}

static void test_battery_protocol_real_frame()
{
    const uint8_t sources[] = {1, 2, 3};
    for (uint8_t source : sources) {
        uint8_t raw[BatteryProtocol::FrameSize] = {};
        make_battery_frame(source, 14.2f, 16.4f, 15.9f, raw);
        BatteryProtocolPacket packet;
        TEST_ASSERT_TRUE(BatteryProtocol::decode(raw, sizeof(raw), packet));
        TEST_ASSERT_EQUAL_UINT8(source, packet.activeMosfet);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.2f, packet.dcVoltage);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.4f, packet.batt0Voltage);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.9f, packet.batt1Voltage);
    }
}

static void test_battery_protocol_rejects_corruption()
{
    uint8_t raw[BatteryProtocol::FrameSize] = {};
    make_battery_frame(1, 14.2f, 16.4f, 15.9f, raw);
    BatteryProtocolPacket packet;

    raw[13] = 3;
    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw), packet));

    make_battery_frame(1, 14.2f, 16.4f, 15.9f, raw);
    float invalid = NAN;
    memcpy(raw + 5, &invalid, sizeof(float));
    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw), packet));

    make_battery_frame(1, 14.2f, 16.4f, 15.9f, raw);
    invalid = INFINITY;
    memcpy(raw + 1, &invalid, sizeof(float));
    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw), packet));

    make_battery_frame(1, 41.0f, 16.4f, 15.9f, raw);
    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw), packet));

    // El consumidor conserva la última lectura buena cuando la siguiente
    // trama falla: una trama inválida nunca sobrescribe el snapshot.
    make_battery_frame(3, 13.8f, 15.7f, 16.1f, raw);
    BatteryProtocolPacket lastGood;
    TEST_ASSERT_TRUE(BatteryProtocol::decode(raw, sizeof(raw), lastGood));
    BatteryProtocolPacket candidate = lastGood;
    raw[13] = 2;
    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw), candidate));
    TEST_ASSERT_EQUAL_UINT8(3, lastGood.activeMosfet);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.1f, lastGood.batt1Voltage);

    TEST_ASSERT_FALSE(BatteryProtocol::decode(raw, sizeof(raw) - 1, packet));
}

static FlowPidConfig dual_flow_config()
{
    FlowPidConfig c = {};
    c.intervalMs = 100; c.deadband = 0.08f; c.kp = 10.0f; c.ki = 1.0f;
    c.integralLimit = 30.0f; c.maxStep = 32; c.filterAlpha = 0.3f;
    c.minActive = 0.2f; c.kd = 0.0f; c.maxDtMs = 1000; c.sensorStaleMs = 1200;
    c.kickPwm = 700; c.kickMs = 100; c.stallFlowLpm = 0.0f;
    c.restallCooldownMs = 1000; c.stallConfirmMs = 100;
    return c;
}

static FlowPidConfig zero_flow_config()
{
    FlowPidConfig c = dual_flow_config();
    c.intervalMs = 100;
    c.kickPwm = 700;
    c.kickMs = 100;
    c.zeroFlowConfirmSamples = 2;
    c.sensorFaultStopMs = 1500;
    return c;
}

static DualMotorFlowInput zero_flow_input(uint32_t nowMs, uint32_t sampleId,
                                          float measuredFlow)
{
    DualMotorFlowInput input;
    input.nowMs = nowMs;
    input.targetFlow = 4.0f;
    input.measuredFlow = measuredFlow;
    input.flowValid = true;
    input.flowFresh = true;
    input.flowStale = false;
    input.flowAgeMs = 0;
    input.sampleId = sampleId;
    input.maxPwm = 1000;
    return input;
}

static void test_zero_flow_requires_two_fresh_run_samples_after_kick()
{
    DualMotorFlowController controller;
    const FlowPidConfig config = zero_flow_config();

    DualMotorFlowInput input = zero_flow_input(0, 1, 0.0f);
    DualMotorFlowOutput output = controller.update(input, config);
    TEST_ASSERT_TRUE(output.kickActive);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);

    // El cero durante el kick no cuenta.
    input = zero_flow_input(100, 2, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_FALSE(output.kickActive);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);

    // Primera muestra cero en Run: sólo deja una confirmación pendiente.
    input = zero_flow_input(200, 3, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(1, output.zeroFlowCount);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);
    TEST_ASSERT_TRUE(output.virtualPwm > 0);

    // Segunda muestra física consecutiva: enclava y apaga ambas bombas.
    input = zero_flow_input(300, 4, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_ZERO_FLOW, output.fault);
    TEST_ASSERT_TRUE(output.zeroFlowFailureLatched);
    TEST_ASSERT_EQUAL_INT(0, output.virtualPwm);
    TEST_ASSERT_EQUAL_INT(0, output.motor0Pwm);
    TEST_ASSERT_EQUAL_INT(0, output.motor1Pwm);
    TEST_ASSERT_TRUE(output.zeroFlowFailurePwm > 0);
}

static void test_zero_flow_confirmation_resets_and_latches_until_reset()
{
    DualMotorFlowController controller;
    const FlowPidConfig config = zero_flow_config();

    DualMotorFlowInput input = zero_flow_input(0, 1, 1.0f);
    controller.update(input, config);
    input = zero_flow_input(100, 2, 1.0f);
    controller.update(input, config); // kick -> Run

    input = zero_flow_input(200, 3, 0.0f);
    DualMotorFlowOutput output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(1, output.zeroFlowCount);

    // Flujo positivo rompe la consecutividad.
    input = zero_flow_input(300, 4, 1.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);

    // Un cero durante kick y una muestra inválida no cuentan.
    input = zero_flow_input(400, 5, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(1, output.zeroFlowCount);
    input = zero_flow_input(500, 6, 0.0f);
    input.flowValid = false;
    input.flowFresh = false;
    input.flowStale = true;
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_SENSOR_INVALID, output.fault);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);

    // Dos ceros frescos posteriores vuelven a confirmar.
    input = zero_flow_input(600, 7, 0.0f);
    controller.update(input, config);
    input = zero_flow_input(700, 8, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_ZERO_FLOW, output.fault);

    // Una muestra positiva posterior no puede reactivar hasta reset().
    input = zero_flow_input(800, 9, 2.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_ZERO_FLOW, output.fault);
    TEST_ASSERT_EQUAL_INT(0, output.virtualPwm);

    controller.reset();
    input = zero_flow_input(900, 10, 2.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);
    TEST_ASSERT_TRUE(output.kickActive);
}

static void test_zero_flow_ignores_disabled_pwm_and_inactive_target()
{
    DualMotorFlowController controller;
    FlowPidConfig config = zero_flow_config();
    config.kickPwm = 0;

    DualMotorFlowInput input = zero_flow_input(0, 1, 0.0f);
    DualMotorFlowOutput output = controller.update(input, config);
    TEST_ASSERT_TRUE(output.kickActive);
    input = zero_flow_input(100, 2, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);
    input = zero_flow_input(200, 3, 0.0f);
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);
    TEST_ASSERT_EQUAL_INT(FLOW_PID_FAULT_NONE, output.fault);

    // Un objetivo inactivo reinicia cualquier confirmación pendiente.
    config.kickPwm = 700;
    input = zero_flow_input(300, 4, 0.0f);
    input.targetFlow = 0.0f;
    output = controller.update(input, config);
    TEST_ASSERT_EQUAL_UINT8(0, output.zeroFlowCount);
    TEST_ASSERT_EQUAL_INT(0, output.virtualPwm);
}

static void test_dual_flow_virtual_mapping_and_sensor_grace()
{
    int primary = 0, secondary = 0;
    DualMotorFlowController::motorsFromVirtual(1300, 1000, primary, secondary);
    TEST_ASSERT_EQUAL_INT(1000, primary);
    TEST_ASSERT_EQUAL_INT(300, secondary);
    TEST_ASSERT_EQUAL_INT(1300, DualMotorFlowController::virtualFromMotors(1000, 300, 1000));

    DualMotorFlowController controller;
    DualMotorFlowInput input;
    input.nowMs = 0; input.targetFlow = 4.0f; input.maxPwm = 1000;
    input.primaryMotor = 1; input.hasFeedForward = true;
    input.feedForwardPrimary = 1000; input.feedForwardSecondary = 250;
    DualMotorFlowOutput output = controller.update(input, dual_flow_config());
    TEST_ASSERT_EQUAL_INT(250, output.motor0Pwm);
    TEST_ASSERT_EQUAL_INT(1000, output.motor1Pwm);
    FlowPidConfig safetyConfig = dual_flow_config();
    safetyConfig.sensorFaultStopMs = 1500;
    input.nowMs = 1499;
    output = controller.update(input, safetyConfig);
    TEST_ASSERT_TRUE(output.sensorGraceActive);
    input.nowMs = 1500;
    output = controller.update(input, safetyConfig);
    TEST_ASSERT_TRUE(output.stoppedForSensorFault);
    TEST_ASSERT_EQUAL_INT(0, output.motor0Pwm);
    TEST_ASSERT_EQUAL_INT(0, output.motor1Pwm);
}

static void test_smart_flow_seeks_and_locks_center_on_crossing()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.15f;
    tune.seekStep = 32;
    tune.maxStep = 32;
    tune.fastAlpha = 1.0f; // Filtro transparente para test determinista
    controller.setTune(tune);

    int pwm = 0;
    SmartFlowStatus status;

    // Paso 1: Flujo bajo (2.0 L/min), buscando centro
    status = controller.update(100, pwm, 5.0f, 2.0f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_SEEK_CENTER), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(pwm >= 30);

    // Paso 2: Flujo sube (4.0 L/min) pero sigue fuera de deadband
    status = controller.update(200, pwm, 5.0f, 4.0f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_TRUE(pwm > 30);

    // Paso 3: Flujo entra a la zona objetivo (5.05 L/min, error = 0.05) -> Tick 1 de estabilización
    status = controller.update(300, pwm, 5.0f, 5.05f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound); // Aún no enclava en el primer tick

    // Paso 4: Flujo sigue en zona objetivo (5.02 L/min) -> Tick 2 de estabilización
    status = controller.update(400, pwm, 5.0f, 5.02f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound); // Aún no enclava en el segundo tick

    // Paso 5: Flujo confirmado estable (4.98 L/min) -> Tick 3 de estabilización -> ¡ENCLAVE!
    status = controller.update(500, pwm, 5.0f, 4.98f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_CENTER_LOCKED), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(status.centerPwm > 0);
    TEST_ASSERT_EQUAL_INT(status.centerPwm, status.pwm);
}

static void test_smart_flow_rejects_startup_spike_and_locks_only_at_target()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.15f;
    tune.fastAlpha = 1.0f;
    controller.setTune(tune);

    int pwm = 1750;
    SmartFlowStatus status;

    // Simula sobrepico tras kick o transitorio: el flujo sube a 6.43 L/min
    // Paso 1: Flujo en 6.43 L/min, target 5.0 L/min -> NO debe enclavar centro, debe bajar PWM
    status = controller.update(100, pwm, 5.0f, 6.43f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_SEEK_CENTER), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(pwm < 1750); // Comprueba que baja el PWM

    // Paso 2: Flujo bajando a 5.8 L/min -> Sigue fuera de deadband, sigue bajando
    status = controller.update(200, pwm, 5.0f, 5.8f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_TRUE(pwm < 1740);

    // Paso 3: Flujo entra en la zona objetivo (5.10 L/min) -> Tick 1 de estabilización
    status = controller.update(300, pwm, 5.0f, 5.10f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);

    // Paso 4: Flujo sigue en zona objetivo (5.02 L/min) -> Tick 2
    status = controller.update(400, pwm, 5.0f, 5.02f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);

    // Paso 5: Flujo confirmado estable (4.99 L/min) -> Tick 3 -> ¡ENCLAVE!
    status = controller.update(500, pwm, 5.0f, 4.99f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_CENTER_LOCKED), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(status.centerPwm > 0);
    TEST_ASSERT_EQUAL_INT(status.centerPwm, status.pwm);
}

static void test_smart_flow_deadband_holds_exact_center_pwm()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.15f;
    tune.fastAlpha = 1.0f;
    controller.setTune(tune);
    controller.seedCenter(750);

    // Flujo dentro de la zona muerta (5.0f +- 0.15f)
    SmartFlowStatus status = controller.update(100, 750, 5.0f, 5.05f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(750, status.centerPwm);
    TEST_ASSERT_EQUAL_INT(750, status.pwm);
    TEST_ASSERT_EQUAL_INT(0, status.trimPwm);

    status = controller.update(200, 750, 5.0f, 4.92f, 0.1f, 2047);
    TEST_ASSERT_EQUAL_INT(750, status.pwm);
    TEST_ASSERT_EQUAL_INT(0, status.trimPwm);
}

static void test_smart_flow_soft_trim_limits_excursion_and_step()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.10f;
    tune.softTrimMax = 40;
    tune.softStepLimit = 2;
    tune.kp = 100.0f; // Ganancia alta para forzar intento de gran salto
    tune.fastAlpha = 1.0f;
    tune.recenterDelayMs = 0; // Desactivar recentering para test puro de trim
    controller.setTune(tune);
    controller.seedCenter(800);

    // Error grande fuera de deadband (flujo = 3.0f, target = 5.0f)
    SmartFlowStatus status = controller.update(100, 800, 5.0f, 3.0f, 0.1f, 2047);
    // Debe limitar el paso a softStepLimit = 2
    TEST_ASSERT_EQUAL_INT(802, status.pwm);
    TEST_ASSERT_EQUAL_INT(2, status.trimPwm);

    // Tras múltiples pasos, nunca debe exceder softTrimMax = 40
    int pwm = status.pwm;
    for (int i = 0; i < 50; ++i)
    {
        status = controller.update(200 + i * 100, pwm, 5.0f, 3.0f, 0.1f, 2047);
        pwm = status.pwm;
    }
    TEST_ASSERT_TRUE(status.trimPwm <= 40);
    TEST_ASSERT_TRUE(status.pwm <= 840);
}

static void test_smart_flow_sensitivity_scaling()
{
    SmartFlowController ctrlNormal, ctrlSoft;
    SmartFlowTune tuneNorm, tuneSoft;
    tuneNorm.deadband = 0.05f;
    tuneNorm.softStepLimit = 20;
    tuneNorm.sensitivity = 1.0f;
    tuneNorm.fastAlpha = 1.0f;
    ctrlNormal.setTune(tuneNorm);
    ctrlNormal.seedCenter(800);

    tuneSoft = tuneNorm;
    tuneSoft.sensitivity = 0.5f;
    ctrlSoft.setTune(tuneSoft);
    ctrlSoft.seedCenter(800);

    SmartFlowStatus statNorm = ctrlNormal.update(100, 800, 5.0f, 4.5f, 0.1f, 2047);
    SmartFlowStatus statSoft = ctrlSoft.update(100, 800, 5.0f, 4.5f, 0.1f, 2047);

    TEST_ASSERT_TRUE(statNorm.trimPwm > statSoft.trimPwm);
}

static void test_smart_flow_recenter_on_sustained_drift()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.10f;
    tune.recenterDelayMs = 1000;
    tune.recenterStep = 2;
    tune.fastAlpha = 1.0f;
    controller.setTune(tune);
    controller.seedCenter(800);

    // Deriva sostenida por debajo del target durante 1500 ms
    SmartFlowStatus status;
    status = controller.update(0, 800, 5.0f, 4.5f, 0.1f, 2047);
    TEST_ASSERT_EQUAL_INT(800, status.centerPwm);

    status = controller.update(500, status.pwm, 5.0f, 4.5f, 0.5f, 2047);
    TEST_ASSERT_EQUAL_INT(800, status.centerPwm);

    // A los 1100 ms se supera recenterDelayMs (1000 ms), recenter incrementa centerPwm
    status = controller.update(1100, status.pwm, 5.0f, 4.5f, 0.6f, 2047);
    TEST_ASSERT_EQUAL_INT(802, status.centerPwm);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_RECENTER), static_cast<int>(status.mode));
}

static void test_smart_flow_fluid_transition_on_target_change()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.10f;
    tune.seekStep = 20;
    tune.maxStep = 20;
    tune.fastAlpha = 1.0f;
    controller.setTune(tune);

    int pwm = 0;
    SmartFlowStatus status;

    // Etapa 1: Target = 1.0 L/min, busca y enclava en 1.0 L/min
    status = controller.update(100, pwm, 1.0f, 0.5f, 0.1f, 2047);
    pwm = status.pwm;
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_SEEK_CENTER), static_cast<int>(status.mode));

    // Estabiliza 3 ticks en 1.0 L/min -> Enclava centro en PWM 600
    status = controller.update(200, 600, 1.0f, 1.02f, 0.1f, 2047);
    status = controller.update(300, 600, 1.0f, 1.00f, 0.1f, 2047);
    status = controller.update(400, 600, 1.0f, 0.99f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(600, status.centerPwm);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_CENTER_LOCKED), static_cast<int>(status.mode));

    // Etapa 2: Cambio de target a 2.0 L/min en caliente desde PWM = 600
    status = controller.update(500, 600, 2.0f, 1.00f, 0.1f, 2047);
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_SEEK_CENTER), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(status.pwm > 600);
    TEST_ASSERT_TRUE(status.pwm <= 620);

    // Continúa subiendo fluidamente hacia el nuevo target
    pwm = status.pwm;
    status = controller.update(600, pwm, 2.0f, 1.50f, 0.1f, 2047);
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_TRUE(status.pwm > pwm);

    // Flujo llega a 2.0 L/min y se estabiliza 3 ticks en PWM = 850
    status = controller.update(700, 850, 2.0f, 2.02f, 0.1f, 2047);
    status = controller.update(800, 850, 2.0f, 2.00f, 0.1f, 2047);
    status = controller.update(900, 850, 2.0f, 1.99f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(850, status.centerPwm);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_CENTER_LOCKED), static_cast<int>(status.mode));
}

static void test_smart_flow_uncenters_on_close_targets()
{
    SmartFlowController controller;
    SmartFlowTune tune;
    tune.deadband = 0.10f;
    tune.seekStep = 16;
    tune.fastAlpha = 1.0f;
    controller.setTune(tune);
    controller.seedCenter(800);

    // Con centro enclavado en 800 para 2.0 L/min
    SmartFlowStatus status = controller.update(100, 800, 2.0f, 2.0f, 0.1f, 2047);
    TEST_ASSERT_TRUE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(800, status.centerPwm);

    // Cambio cercano: de 2.0 a 2.3 L/min (diferencia 0.3 L/min < breakoutThreshold de 2.0)
    status = controller.update(200, 800, 2.3f, 2.0f, 0.1f, 2047);
    TEST_ASSERT_FALSE(status.centerFound);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SmartFlowMode::SMART_FLOW_SEEK_CENTER), static_cast<int>(status.mode));
    TEST_ASSERT_TRUE(status.pwm > 800);
}

// Copia del barrido del perfil Dron V2 (src/Variants/Profiles/Dron.h); el suite
// nativo compila con el perfil Legacy y no puede incluir ambos.
static const FlowFeedForwardPoint kTestFeedForward[] = {
    {184, 0.00f}, {410, 0.80f}, {594, 1.71f}, {819, 2.51f}, {1003, 3.11f},
    {1228, 3.78f}, {1412, 4.60f}, {1638, 5.28f}, {1823, 5.87f}, {2047, 6.58f},
};

static FlowPidConfig dron_v2_flow_config(bool withFeedForward)
{
    FlowPidConfig c{};
    c.intervalMs = 100; c.deadband = 0.08f; c.kp = 20.0f; c.ki = 0.8f;
    c.integralLimit = 20.0f; c.maxStep = 24; c.filterAlpha = 0.30f;
    c.minActive = 0.20f; c.kd = 0.0f; c.maxDtMs = 1000; c.sensorStaleMs = 1200;
    c.kickPwm = 800; c.kickMs = 250; c.stallFlowLpm = 0.10f;
    c.restallCooldownMs = 8000; c.stallConfirmMs = 1500; c.softTrimMax = 80;
    c.softMaxStep = 2; c.sensitivity = 1.0f; c.recenterDelayMs = 4000;
    c.zeroFlowConfirmSamples = 5;
    if (withFeedForward)
    {
        c.feedForward = {kTestFeedForward, 10};
        c.rampCountsPerSecond = 1000.0f;
    }
    return c;
}

// Planta de una bomba: rampa física de 1 cuenta/ms tras el mando, tiempo
// muerto, retardo de primer orden, umbral de arranque desde reposo y AFM07
// cuantizado a 0,01 L/min cada 250 ms con ruido pseudoaleatorio opcional.
struct PumpPlantSim
{
    float gainScale = 1.0f;
    float deadTimeS = 0.30f;
    float tauS = 0.40f;
    float noiseLpm = 0.0f;
    int breakawayPwm = 0;     // PWM físico necesario para arrancar desde reposo
    bool blocked = false;
    int command = 0;
    float physicalPwm = 0.0f;
    float flow = 0.0f;
    bool spinning = false;
    float history[160] = {};
    uint8_t head = 0;
    uint32_t noiseState = 12345u;

    float staticFlow(float pwm) const
    {
        if (blocked || !spinning || pwm <= kTestFeedForward[0].pwm) return 0.0f;
        for (int i = 1; i < 10; ++i)
        {
            if (pwm <= kTestFeedForward[i].pwm)
            {
                const FlowFeedForwardPoint &a = kTestFeedForward[i - 1];
                const FlowFeedForwardPoint &b = kTestFeedForward[i];
                const float t = (pwm - a.pwm) / static_cast<float>(b.pwm - a.pwm);
                return gainScale * (a.flowLpm + t * (b.flowLpm - a.flowLpm));
            }
        }
        return gainScale * kTestFeedForward[9].flowLpm;
    }

    // Avanza 10 ms con el último PWM comandado.
    void step()
    {
        const float dt = 0.010f;
        if (physicalPwm < command) physicalPwm = fminf(physicalPwm + 10.0f, command);
        else physicalPwm = fmaxf(physicalPwm - 10.0f, command);
        history[head] = physicalPwm;
        head = (head + 1) % 160;
        const int delaySteps = static_cast<int>(deadTimeS / dt);
        const float delayedPwm = history[(head + 160 - 1 - delaySteps) % 160];
        if (delayedPwm <= kTestFeedForward[0].pwm) spinning = false;
        else if (delayedPwm >= breakawayPwm) spinning = true;
        flow += (staticFlow(delayedPwm) - flow) * (dt / tauS);
    }

    float reading()
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        const float unit = static_cast<float>(noiseState >> 8) / 16777216.0f; // [0, 1)
        const float y = flow > 0.005f ? flow + noiseLpm * (2.0f * unit - 1.0f) : 0.0f;
        return roundf(fmaxf(y, 0.0f) * 100.0f) / 100.0f;
    }
};

struct FlowSimResult
{
    float lockSeconds = -1.0f;
    float maxFlow = 0.0f;           // Flujo real (sin ruido de medida)
    float minFlow = 99.0f;
    float minFlowAfterLock = 99.0f;
    float maxFlowAfterLock = 0.0f;
    bool zeroFlowLatched = false;
};

// Ejecuta el lazo completo (DualMotorFlowController) desde startMs hasta
// startMs + durationMs con una consigna fija. El tiempo de enclave se mide
// desde startMs.
static FlowSimResult run_flow_sim(DualMotorFlowController &controller, PumpPlantSim &plant,
                                  const FlowPidConfig &config, float target,
                                  uint32_t startMs, uint32_t durationMs, uint32_t &sampleId)
{
    FlowSimResult result;
    for (uint32_t t = startMs; t < startMs + durationMs; t += 10)
    {
        plant.step();
        if ((t % 250) != 0) continue;
        DualMotorFlowInput input;
        input.nowMs = t;
        input.targetFlow = target;
        input.measuredFlow = plant.reading();
        input.flowValid = true;
        input.flowFresh = true;
        input.sampleId = ++sampleId;
        input.maxPwm = 2047;
        const DualMotorFlowOutput out = controller.update(input, config);
        plant.command = out.motor0Pwm;
        const float y = plant.flow;
        if (out.zeroFlowFailureLatched) result.zeroFlowLatched = true;
        if (y > result.maxFlow) result.maxFlow = y;
        if (y < result.minFlow) result.minFlow = y;
        if (result.lockSeconds < 0.0f && out.smartStatus.centerFound)
            result.lockSeconds = (t - startMs) / 1000.0f;
        if (result.lockSeconds >= 0.0f && (t - startMs) / 1000.0f >= result.lockSeconds + 2.0f)
        {
            if (y < result.minFlowAfterLock) result.minFlowAfterLock = y;
            if (y > result.maxFlowAfterLock) result.maxFlowAfterLock = y;
        }
    }
    return result;
}

static void test_feed_forward_table_inverse_and_validation()
{
    const FlowFeedForwardTable table = {kTestFeedForward, 10};
    TEST_ASSERT_TRUE(table.valid(2047));
    TEST_ASSERT_FALSE(table.valid(2000));
    TEST_ASSERT_EQUAL_INT(410, table.startPwm());
    TEST_ASSERT_EQUAL_INT(184, table.pwmForFlow(0.0f));
    TEST_ASSERT_EQUAL_INT(1228, table.pwmForFlow(3.78f));
    // La consigna enclavada en hardware fue PWM 1695 para 5,48 L/min.
    TEST_ASSERT_INT_WITHIN(15, 1695, table.pwmForFlow(5.48f));
    TEST_ASSERT_EQUAL_INT(2047, table.pwmForFlow(8.0f));

    const FlowFeedForwardPoint descending[] = {{100, 1.0f}, {200, 0.5f}};
    TEST_ASSERT_FALSE((FlowFeedForwardTable{descending, 2}.valid(2047)));

    FlowPidConfig config = dron_v2_flow_config(true);
    TEST_ASSERT_TRUE(FlowMotorController::validateConfig(config, 2047));
    config.feedForward = {descending, 2};
    TEST_ASSERT_FALSE(FlowMotorController::validateConfig(config, 2047));
}

static void test_feed_forward_limits_start_kick_for_low_targets()
{
    const FlowPidConfig config = dron_v2_flow_config(true);
    TEST_ASSERT_EQUAL_INT(800, FlowMotorController::startKickPwm(config, 5.5f, 2047));
    TEST_ASSERT_EQUAL_INT(594, FlowMotorController::startKickPwm(config, 1.71f, 2047));
    // Nunca por debajo del umbral de arranque medido.
    TEST_ASSERT_EQUAL_INT(410, FlowMotorController::startKickPwm(config, 0.4f, 2047));
    TEST_ASSERT_EQUAL_INT(800, FlowMotorController::startKickPwm(dron_v2_flow_config(false), 0.4f, 2047));
}

// Planta realista por defecto: ruido de medida ±0,04 L/min (dispersión vista
// en mantenimiento) y arranque desde reposo solo sobre ~350 cuentas físicas.
static PumpPlantSim realistic_plant(float gain, float deadTimeS, float tauS)
{
    PumpPlantSim plant;
    plant.gainScale = gain;
    plant.deadTimeS = deadTimeS;
    plant.tauS = tauS;
    plant.noiseLpm = 0.04f;
    plant.breakawayPwm = 350;
    return plant;
}

// Tolerancia de enclave: la banda de búsqueda es absoluta (±0,10 L/min), así
// que en consignas bajas domina sobre el 3 %.
static float flow_tolerance(float target)
{
    return fmaxf(0.03f * target, 0.12f);
}

static void test_feed_forward_approach_locks_fast_without_overshoot()
{
    const float deadTimes[] = {0.3f, 0.6f};
    const float taus[] = {0.4f, 0.9f};
    const float gains[] = {0.85f, 1.0f, 1.15f};
    const float targets[] = {1.0f, 2.0f, 4.0f, 5.5f};
    for (float deadTime : deadTimes)
        for (float tau : taus)
            for (float gain : gains)
                for (float target : targets)
                {
                    DualMotorFlowController controller;
                    PumpPlantSim plant = realistic_plant(gain, deadTime, tau);
                    uint32_t sampleId = 0;
                    const FlowSimResult r = run_flow_sim(controller, plant, dron_v2_flow_config(true),
                                                         target, 0, 25000, sampleId);
                    char message[112];
                    snprintf(message, sizeof(message),
                             "theta %.1f tau %.1f gain %.2f target %.1f lock %.2fs max %.2f",
                             deadTime, tau, gain, target, r.lockSeconds, r.maxFlow);
                    TEST_ASSERT_FALSE_MESSAGE(r.zeroFlowLatched, message);
                    TEST_ASSERT_TRUE_MESSAGE(r.lockSeconds > 0.0f && r.lockSeconds < 12.0f, message);
                    if (deadTime == 0.3f && tau == 0.4f && gain == 1.0f)
                    {
                        TEST_MESSAGE(message);
                        TEST_ASSERT_TRUE_MESSAGE(r.lockSeconds < 6.0f, message);
                    }
                    TEST_ASSERT_TRUE_MESSAGE(r.maxFlow <= target + flow_tolerance(target), message);
                    TEST_ASSERT_TRUE_MESSAGE(fabsf(r.minFlowAfterLock - target) <= 0.2f &&
                                                 fabsf(r.maxFlowAfterLock - target) <= 0.2f, message);
                }
}

static void test_feed_forward_low_target_starts_pump_without_zero_flow()
{
    const float gains[] = {0.85f, 1.0f, 1.15f};
    const float targets[] = {0.5f, 1.0f};
    for (float gain : gains)
        for (float target : targets)
        {
            DualMotorFlowController controller;
            PumpPlantSim plant = realistic_plant(gain, 0.3f, 0.4f);
            uint32_t sampleId = 0;
            const FlowSimResult r = run_flow_sim(controller, plant, dron_v2_flow_config(true),
                                                 target, 0, 25000, sampleId);
            char message[96];
            snprintf(message, sizeof(message), "gain %.2f target %.1f lock %.2fs max %.2f",
                     gain, target, r.lockSeconds, r.maxFlow);
            TEST_ASSERT_FALSE_MESSAGE(r.zeroFlowLatched, message);
            TEST_ASSERT_TRUE_MESSAGE(r.lockSeconds > 0.0f, message);
            TEST_ASSERT_TRUE_MESSAGE(fabsf(r.maxFlowAfterLock - target) <= 0.2f, message);
        }
}

static void test_feed_forward_approach_handles_target_changes()
{
    const float deadTimes[] = {0.3f, 0.6f};
    const float taus[] = {0.4f, 0.9f};
    const float gains[] = {0.85f, 1.0f, 1.15f};
    for (float deadTime : deadTimes)
        for (float tau : taus)
            for (float gain : gains)
            {
                DualMotorFlowController controller;
                PumpPlantSim plant = realistic_plant(gain, deadTime, tau);
                uint32_t sampleId = 0;
                const FlowPidConfig config = dron_v2_flow_config(true);
                run_flow_sim(controller, plant, config, 5.5f, 0, 20000, sampleId);
                const float steps[][2] = {{5.5f, 2.0f}, {2.0f, 4.0f}, {4.0f, 5.5f}};
                uint32_t start = 20000;
                for (const auto &step : steps)
                {
                    const FlowSimResult r = run_flow_sim(controller, plant, config, step[1],
                                                         start, 20000, sampleId);
                    start += 20000;
                    char message[112];
                    snprintf(message, sizeof(message),
                             "theta %.1f tau %.1f gain %.2f %.1f->%.1f lock %.2fs min %.2f max %.2f",
                             deadTime, tau, gain, step[0], step[1], r.lockSeconds, r.minFlow, r.maxFlow);
                    TEST_ASSERT_TRUE_MESSAGE(r.lockSeconds > 0.0f && r.lockSeconds < 12.0f, message);
                    // Sin cruzar la consigna en el sentido del cambio.
                    if (step[1] > step[0])
                        TEST_ASSERT_TRUE_MESSAGE(r.maxFlow <= step[1] + flow_tolerance(step[1]), message);
                    else
                        TEST_ASSERT_TRUE_MESSAGE(r.minFlow >= step[1] - flow_tolerance(step[1]), message);
                    TEST_ASSERT_TRUE_MESSAGE(fabsf(r.minFlowAfterLock - step[1]) <= 0.2f &&
                                                 fabsf(r.maxFlowAfterLock - step[1]) <= 0.2f, message);
                }
            }
}

static void test_feed_forward_is_faster_than_ramp_seek_and_keeps_zero_flow_guard()
{
    DualMotorFlowController seekController;
    PumpPlantSim seekPlant = realistic_plant(1.0f, 0.3f, 0.4f);
    uint32_t sampleId = 0;
    const FlowSimResult seek = run_flow_sim(seekController, seekPlant, dron_v2_flow_config(false),
                                            5.5f, 0, 40000, sampleId);
    DualMotorFlowController ffController;
    PumpPlantSim ffPlant = realistic_plant(1.0f, 0.3f, 0.4f);
    sampleId = 0;
    const FlowSimResult ff = run_flow_sim(ffController, ffPlant, dron_v2_flow_config(true),
                                          5.5f, 0, 40000, sampleId);
    char message[64];
    snprintf(message, sizeof(message), "seek %.2fs feed-forward %.2fs", seek.lockSeconds, ff.lockSeconds);
    TEST_MESSAGE(message);
    TEST_ASSERT_TRUE_MESSAGE(seek.lockSeconds > 10.0f, message);
    TEST_ASSERT_TRUE_MESSAGE(ff.lockSeconds > 0.0f && ff.lockSeconds * 2.0f < seek.lockSeconds, message);

    DualMotorFlowController blockedController;
    PumpPlantSim blocked = realistic_plant(1.0f, 0.3f, 0.4f);
    blocked.blocked = true;
    sampleId = 0;
    const FlowSimResult r = run_flow_sim(blockedController, blocked, dron_v2_flow_config(true),
                                         5.5f, 0, 5000, sampleId);
    TEST_ASSERT_TRUE(r.zeroFlowLatched);
}

static void test_flow_motor_controller_no_kick_on_running_target_change()
{
    FlowMotorController controller;
    FlowPidConfig config;
    config.intervalMs = 100;
    config.deadband = 0.10f;
    config.kp = 14.0f;
    config.ki = 0.4f;
    config.integralLimit = 16.0f;
    config.maxStep = 16;
    config.filterAlpha = 1.0f;
    config.minActive = 0.20f;
    config.kd = 0.0f;
    config.maxDtMs = 2000;
    config.sensorStaleMs = 2000;
    config.kickPwm = 1500;
    config.kickMs = 200;
    config.stallFlowLpm = 0.1f;
    config.restallCooldownMs = 5000;
    config.stallConfirmMs = 500;
    config.softTrimMax = 64;
    config.softMaxStep = 2;
    config.sensitivity = 1.0f;
    config.recenterDelayMs = 4000;

    FlowMotorInput input;
    input.nowMs = 0;
    input.currentPwm = 0;
    input.targetFlow = 1.0f;
    input.measuredFlow = 0.0f;
    input.flowValid = true;
    input.flowFresh = true;
    input.maxPwm = 2047;

    // 1. Kick inicial
    FlowMotorOutput out = controller.update(input, config);
    TEST_ASSERT_TRUE(out.kickActive);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IgnitionPhase::Kick), static_cast<int>(out.ignitionPhase));
    TEST_ASSERT_EQUAL_UINT16(1, out.kickCount);

    // 2. Transición a Run tras kickMs
    input.nowMs = 250;
    input.currentPwm = out.pwm;
    input.measuredFlow = 0.8f;
    out = controller.update(input, config);
    TEST_ASSERT_FALSE(out.kickActive);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IgnitionPhase::Run), static_cast<int>(out.ignitionPhase));
    TEST_ASSERT_EQUAL_UINT16(1, out.kickCount);

    // Estabiliza en 1.0 L/min
    input.nowMs = 350;
    input.currentPwm = 600;
    input.measuredFlow = 1.0f;
    out = controller.update(input, config);

    input.nowMs = 450;
    input.currentPwm = 600;
    input.measuredFlow = 1.0f;
    out = controller.update(input, config);

    input.nowMs = 550;
    input.currentPwm = 600;
    input.measuredFlow = 1.0f;
    out = controller.update(input, config);
    TEST_ASSERT_TRUE(out.smartStatus.centerFound);

    // 3. Cambio de target a 2.0 L/min en marcha
    input.nowMs = 650;
    input.targetFlow = 2.0f;
    input.currentPwm = 600;
    input.measuredFlow = 1.0f;
    out = controller.update(input, config);

    // NO debe hacer kick, debe seguir en Run con kickCount = 1
    TEST_ASSERT_FALSE(out.kickActive);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IgnitionPhase::Run), static_cast<int>(out.ignitionPhase));
    TEST_ASSERT_EQUAL_UINT16(1, out.kickCount);
    TEST_ASSERT_FALSE(out.smartStatus.centerFound);
    TEST_ASSERT_TRUE(out.pwm > 600);
}

static void test_dual_motor_multi_stage_seamless_pwm()
{
    DualMotorFlowController dual;
    FlowPidConfig config;
    config.intervalMs = 100;
    config.deadband = 0.10f;
    config.kp = 14.0f;
    config.ki = 0.4f;
    config.integralLimit = 16.0f;
    config.maxStep = 16;
    config.filterAlpha = 1.0f;
    config.minActive = 0.20f;
    config.kd = 0.0f;
    config.maxDtMs = 2000;
    config.sensorStaleMs = 2000;
    config.kickPwm = 1500;
    config.kickMs = 200;
    config.stallFlowLpm = 0.0f;
    config.restallCooldownMs = 5000;
    config.stallConfirmMs = 500;
    config.softTrimMax = 64;
    config.softMaxStep = 2;
    config.sensitivity = 1.0f;
    config.recenterDelayMs = 4000;

    DualMotorFlowInput in;
    in.nowMs = 0;
    in.targetFlow = 1.0f;
    in.measuredFlow = 0.0f;
    in.flowValid = true;
    in.flowFresh = true;
    in.sampleId = 1;
    in.maxPwm = 2047;
    in.primaryMotor = 0;
    in.hasFeedForward = false;

    // Inicio con Kick
    DualMotorFlowOutput out = dual.update(in, config);
    TEST_ASSERT_TRUE(out.kickActive);

    // Pasa a Run
    in.nowMs = 250;
    in.sampleId = 2;
    in.measuredFlow = 0.95f;
    out = dual.update(in, config);
    TEST_ASSERT_FALSE(out.kickActive);

    int runningPwm = out.virtualPwm;
    TEST_ASSERT_TRUE(runningPwm > 0);

    // Cambio de etapa: target 2.0 L/min
    in.nowMs = 350;
    in.sampleId = 3;
    in.targetFlow = 2.0f;
    in.measuredFlow = 1.0f;
    out = dual.update(in, config);

    // Continuidad: no debe caer a 0 ni reiniciar kick
    TEST_ASSERT_FALSE(out.kickActive);
    TEST_ASSERT_TRUE(out.virtualPwm >= runningPwm);
}

static void test_http_url_normalize_adds_path_and_reports_errors()
{
    char out[32];
    const char *error = nullptr;
    TEST_ASSERT_TRUE(HttpUrl::normalize("http://a.cl", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("http://a.cl/", out);
    TEST_ASSERT_TRUE(HttpUrl::normalize("https://a.cl/x?y=1", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("https://a.cl/x?y=1", out);

    TEST_ASSERT_FALSE(HttpUrl::normalize("ftp://a.cl", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("URL debe iniciar con http:// o https://", error);
    TEST_ASSERT_FALSE(HttpUrl::normalize("http:///x", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("URL sin host", error);
    TEST_ASSERT_FALSE(HttpUrl::normalize("http://a.cl/abcdefghijklmnop", out, 16, error));
    TEST_ASSERT_EQUAL_STRING("URL demasiado larga", error);
    TEST_ASSERT_FALSE(HttpUrl::normalize(nullptr, out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("URL HTTP inválida", error);
}

static void test_http_url_host_and_ip_rewrite()
{
    char host[16];
    TEST_ASSERT_TRUE(HttpUrl::extractHost("http://time.cmasccp.cl:80/p", host, sizeof(host)));
    TEST_ASSERT_EQUAL_STRING("time.cmasccp.cl", host);
    TEST_ASSERT_FALSE(HttpUrl::extractHost("http://time.cmasccp.cl/", host, 8));
    TEST_ASSERT_FALSE(HttpUrl::extractHost("ftp://x", host, sizeof(host)));

    char out[40];
    const char *error = nullptr;
    TEST_ASSERT_TRUE(HttpUrl::withHostIp("http://a.cl/p?q=1", "1.2.3.4", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("http://1.2.3.4/p?q=1", out);
    TEST_ASSERT_TRUE(HttpUrl::withHostIp("http://a.cl", "1.2.3.4", out, sizeof(out), error));
    TEST_ASSERT_EQUAL_STRING("http://1.2.3.4/", out);
    TEST_ASSERT_FALSE(HttpUrl::withHostIp("https://a.cl/", "1.2.3.4", out, sizeof(out), error));
    TEST_ASSERT_NULL(error);
    TEST_ASSERT_FALSE(HttpUrl::withHostIp("http://a.cl/long/path", "1.2.3.4", out, 12, error));
    TEST_ASSERT_EQUAL_STRING("URL IP demasiado larga", error);
}

static void test_http_url_ipv4_validation()
{
    TEST_ASSERT_TRUE(HttpUrl::isValidIPv4("10.0.255.1"));
    TEST_ASSERT_FALSE(HttpUrl::isValidIPv4("256.0.0.1"));
    TEST_ASSERT_FALSE(HttpUrl::isValidIPv4("1.2.3"));
    TEST_ASSERT_FALSE(HttpUrl::isValidIPv4("1.2.3.4.5"));
    TEST_ASSERT_FALSE(HttpUrl::isValidIPv4("1..3.4"));
    TEST_ASSERT_FALSE(HttpUrl::isValidIPv4(""));
}

static void test_at_response_registration_status()
{
    TEST_ASSERT_TRUE(AtResponse::isRegistered("+CREG: 0,1\r\nOK"));
    TEST_ASSERT_TRUE(AtResponse::isRegistered("+CGREG: 2,5"));
    TEST_ASSERT_TRUE(AtResponse::isRegistered("+CEREG: 1"));
    TEST_ASSERT_FALSE(AtResponse::isRegistered("+CREG: 0,2"));
    TEST_ASSERT_FALSE(AtResponse::isRegistered("+CREG: 0,1\r\nERROR"));
    TEST_ASSERT_FALSE(AtResponse::isRegistered("sin dos puntos"));
    TEST_ASSERT_FALSE(AtResponse::isRegistered(nullptr));
}

static void test_at_response_ip_address_detection()
{
    TEST_ASSERT_TRUE(AtResponse::hasValidIp("+IPADDR: 10.20.30.40\r\nOK"));
    TEST_ASSERT_FALSE(AtResponse::hasValidIp("+IPADDR: 999.999"));
    TEST_ASSERT_FALSE(AtResponse::hasValidIp("+IPADDR: 1.2.3"));
    TEST_ASSERT_FALSE(AtResponse::hasValidIp("ERROR 1.2.3.4"));
    TEST_ASSERT_FALSE(AtResponse::hasValidIp(""));
    TEST_ASSERT_EQUAL_STRING("Timeout", AtResponse::httpActionStatusText(705));
    TEST_ASSERT_EQUAL_STRING("", AtResponse::httpActionStatusText(200));
}

static void test_signal_quality_parses_csq_and_maps_bars()
{
    int ber = 0;
    TEST_ASSERT_EQUAL_INT(20, SignalQuality::parseCsq("\r\n+CSQ: 20,3\r\nOK", &ber));
    TEST_ASSERT_EQUAL_INT(3, ber);
    TEST_ASSERT_EQUAL_INT(31, SignalQuality::parseCsq("+CSQ:\t31", &ber));
    TEST_ASSERT_EQUAL_INT(199, SignalQuality::parseCsq("+CSQ: 199,99", &ber));
    TEST_ASSERT_EQUAL_INT(-1, SignalQuality::parseCsq("+CSQ: 200,0", &ber));
    TEST_ASSERT_EQUAL_INT(-1, SignalQuality::parseCsq("ERROR", &ber));
    TEST_ASSERT_EQUAL_INT(-1, SignalQuality::parseCsq(nullptr, &ber));

    TEST_ASSERT_EQUAL_UINT8(0, SignalQuality::barsFromCsq(99, true));
    TEST_ASSERT_EQUAL_UINT8(0, SignalQuality::barsFromCsq(20, false));
    TEST_ASSERT_EQUAL_UINT8(4, SignalQuality::barsFromCsq(20, true));   // -73 dBm
    TEST_ASSERT_EQUAL_UINT8(3, SignalQuality::barsFromCsq(14, true));   // -85 dBm
    TEST_ASSERT_EQUAL_UINT8(2, SignalQuality::barsFromCsq(8, true));    // -97 dBm
    TEST_ASSERT_EQUAL_UINT8(1, SignalQuality::barsFromCsq(2, true));    // -109 dBm
    TEST_ASSERT_EQUAL_UINT8(0, SignalQuality::barsFromCsq(0, true));    // -113 dBm
    TEST_ASSERT_EQUAL_UINT8(4, SignalQuality::barsFromCsq(150, true));  // -66 dBm
    TEST_ASSERT_EQUAL_UINT8(0, SignalQuality::barsFromCsq(50, true));
}

static void test_status_led_temperature_gradient_and_low_power_peak()
{
    using StatusLedPalette::temperatureColor;
    auto green = temperatureColor(25.0f, false);
    TEST_ASSERT_EQUAL_UINT8(0, green.r);
    TEST_ASSERT_EQUAL_UINT8(55, green.g);
    auto lowGreen = temperatureColor(25.0f, true);
    TEST_ASSERT_EQUAL_UINT8(45, lowGreen.g);

    auto lime = temperatureColor(35.0f, false);
    TEST_ASSERT_EQUAL_UINT8(13, lime.r);
    TEST_ASSERT_EQUAL_UINT8(55, lime.g);
    auto yellow = temperatureColor(45.0f, false);
    TEST_ASSERT_EQUAL_UINT8(55, yellow.r);
    TEST_ASSERT_EQUAL_UINT8(55, yellow.g);
    auto orange = temperatureColor(55.0f, false);
    TEST_ASSERT_EQUAL_UINT8(55, orange.r);
    TEST_ASSERT_EQUAL_UINT8(27, orange.g);
    auto red = temperatureColor(60.0f, true);
    TEST_ASSERT_EQUAL_UINT8(45, red.r);
    TEST_ASSERT_EQUAL_UINT8(0, red.g);
}

static void test_status_led_profiles_select_power_mode()
{
    auto normal = StatusLedPalette::profile(StatusLedPattern::Error, false);
    TEST_ASSERT_EQUAL_UINT8(55, normal.primary.r);
    TEST_ASSERT_EQUAL_UINT16(220, normal.onMs);
    auto low = StatusLedPalette::profile(StatusLedPattern::Error, true);
    TEST_ASSERT_EQUAL_UINT8(45, low.primary.r);
    TEST_ASSERT_EQUAL_UINT16(800, low.gapMs);
    TEST_ASSERT_EQUAL_UINT8(3, low.flashes);
    auto off = StatusLedPalette::profile(StatusLedPattern::Off, true);
    TEST_ASSERT_EQUAL_UINT8(0, off.flashes);
    TEST_ASSERT_EQUAL_UINT8(0, off.primary.r);
}

static void test_console_args_read_named_values()
{
    int i = -1;
    float f = -1.0f;
    TEST_ASSERT_TRUE(ConsoleArgs::readInt("set kick=1650 kickMs=300", "kick", i));
    TEST_ASSERT_EQUAL_INT(1650, i);
    TEST_ASSERT_TRUE(ConsoleArgs::readInt("set kick=1650 kickMs=300", "kickMs", i));
    TEST_ASSERT_EQUAL_INT(300, i);
    TEST_ASSERT_TRUE(ConsoleArgs::readFloat("kp=35.5 ki=1", "kp", f));
    TEST_ASSERT_EQUAL_FLOAT(35.5f, f);
    TEST_ASSERT_FALSE(ConsoleArgs::readInt("kp=1", "ki", i));
    // Valor vacio: no debe leer el argumento siguiente.
    TEST_ASSERT_TRUE(ConsoleArgs::readInt("kp= ki=5", "kp", i));
    TEST_ASSERT_EQUAL_INT(0, i);
}

static void test_console_args_parse_pid_config_overrides_only_given_fields()
{
    FlowPidConfig base = EoloConfig::Profile::kFlowPid;
    FlowPidConfig cfg = ConsoleArgs::parsePidConfig("set kp=12.5 interval=250 zeroConfirm=4", base);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, cfg.kp);
    TEST_ASSERT_EQUAL_UINT32(250, cfg.intervalMs);
    TEST_ASSERT_EQUAL_UINT8(4, cfg.zeroFlowConfirmSamples);
    TEST_ASSERT_EQUAL_FLOAT(base.ki, cfg.ki);
    TEST_ASSERT_EQUAL_INT(base.kickPwm, cfg.kickPwm);
}

static void test_console_args_format_duration()
{
    char out[32];
    ConsoleArgs::formatDuration(UINT32_MAX, UINT32_MAX, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("infinita", out);
    ConsoleArgs::formatDuration(42, UINT32_MAX, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("42 s", out);
    ConsoleArgs::formatDuration(187, UINT32_MAX, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("3 min 07 s", out);
    ConsoleArgs::formatDuration(7500, UINT32_MAX, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("2 h 05 min", out);
}

static void test_rtc_to_unix_calendar_boundaries()
{
    uint32_t unixTime = 0;
    TEST_ASSERT_TRUE(RtcTimeParser::toUnix(2000, 1, 1, 0, 0, 0, unixTime));
    TEST_ASSERT_EQUAL_UINT32(946684800UL, unixTime);
    TEST_ASSERT_TRUE(RtcTimeParser::toUnix(2000, 2, 29, 23, 59, 59, unixTime));
    TEST_ASSERT_FALSE(RtcTimeParser::toUnix(2023, 2, 29, 0, 0, 0, unixTime));
    TEST_ASSERT_FALSE(RtcTimeParser::toUnix(2024, 4, 31, 0, 0, 0, unixTime));
    TEST_ASSERT_FALSE(RtcTimeParser::toUnix(2024, 1, 1, 24, 0, 0, unixTime));
    TEST_ASSERT_FALSE(RtcTimeParser::toUnix(2100, 1, 1, 0, 0, 0, unixTime));
}

static void test_rs485_stats_state_transitions()
{
    RS485SlaveStats stats;
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Offline, (int)stats.state);

    // Fallos antes del primer exito: Offline.
    stats.record(false, RS485_TIMEOUT, 0, 100, 250, 0, 0, 350);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Offline, (int)stats.state);
    TEST_ASSERT_EQUAL_UINT32(1, stats.timeoutErrors);

    stats.record(true, RS485_OK, 0, 400, 20, 0, 0, 420);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Online, (int)stats.state);
    TEST_ASSERT_EQUAL_UINT32(0, stats.consecutiveFailures);
    TEST_ASSERT_EQUAL_UINT32(420, stats.lastSuccessMs);

    // 2 fallos siguen Online, el 3 degrada, el 6 da de baja.
    stats.record(false, RS485_INVALID_CRC, 0, 450, 30, 0, 0, 480);
    stats.record(false, RS485_MALFORMED, 0, 500, 30, 0, 0, 530);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Online, (int)stats.state);
    stats.record(false, RS485_BUS_BUSY, 0, 550, 30, 0, 0, 580);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Degraded, (int)stats.state);
    stats.record(false, RS485_INCOMPLETE_FRAME, 0, 600, 30, 0, 0, 630);
    stats.record(false, RS485_UNEXPECTED_FRAME, 0, 650, 30, 0, 0, 680);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Degraded, (int)stats.state);
    stats.record(false, RS485_INVALID_SLAVE, 0, 700, 30, 0, 0, 730);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Offline, (int)stats.state);
    TEST_ASSERT_EQUAL_UINT32(6, stats.consecutiveFailures);
    TEST_ASSERT_EQUAL_UINT32(1, stats.crcErrors);
    TEST_ASSERT_EQUAL_UINT32(1, stats.malformedFrames);
    TEST_ASSERT_EQUAL_UINT32(1, stats.busBusyErrors);
    TEST_ASSERT_EQUAL_UINT32(1, stats.incompleteFrames);
    TEST_ASSERT_EQUAL_UINT32(2, stats.unexpectedFrameErrors);

    // Un exito recupera el endpoint y reinicia la racha.
    stats.record(true, RS485_OK, 0, 750, 20, 0, 0, 770);
    TEST_ASSERT_EQUAL((int)RS485EndpointState::Online, (int)stats.state);
    TEST_ASSERT_EQUAL_UINT32(0, stats.consecutiveFailures);
    TEST_ASSERT_EQUAL_UINT32(350, stats.maxSuccessGapMs);
}

static void test_rs485_stats_exceptions_and_timing()
{
    RS485SlaveStats stats;
    stats.record(true, RS485_OK, 0, 1000, 20, 3, 1, 1020);
    TEST_ASSERT_EQUAL_UINT32(3, stats.lateBytes);
    TEST_ASSERT_EQUAL_UINT32(1, stats.unexpectedFrames);

    stats.record(false, RS485_EXCEPTION, 0x06, 1250, 15, 0, 0, 1265);
    TEST_ASSERT_EQUAL_UINT8(0x06, stats.lastExceptionCode);
    TEST_ASSERT_EQUAL_UINT32(1, stats.modbusExceptions[6]);
    stats.record(false, RS485_EXCEPTION, 0x42, 1500, 15, 0, 0, 1515);
    TEST_ASSERT_EQUAL_UINT32(1, stats.otherExceptionErrors);
    TEST_ASSERT_EQUAL_UINT8(0x42, stats.lastExceptionCode);
    // Un timeout posterior conserva el ultimo codigo de excepcion.
    stats.record(false, RS485_TIMEOUT, 0, 1750, 250, 0, 0, 2000);
    TEST_ASSERT_EQUAL_UINT8(0x42, stats.lastExceptionCode);

    TEST_ASSERT_EQUAL_UINT32(230, stats.minRestMs);
    TEST_ASSERT_EQUAL_UINT32(250, stats.maxAttemptGapMs);
    TEST_ASSERT_EQUAL_UINT32(250, stats.maxLatencyMs);

    TEST_ASSERT_FALSE(RS485SlaveStats::isDeadlineMiss(500, 500));
    TEST_ASSERT_FALSE(RS485SlaveStats::isDeadlineMiss(520, 500));
    TEST_ASSERT_TRUE(RS485SlaveStats::isDeadlineMiss(521, 500));
    // Adelantado respecto al vencimiento no es fallo (diferencia negativa).
    TEST_ASSERT_FALSE(RS485SlaveStats::isDeadlineMiss(480, 500));
}

// Configuración del prototipo medida en hardware (Dron V2, una bomba).
static FlowPidConfig dron_v2_proto_config()
{
    FlowPidConfig c = dron_v2_flow_config(true);
    c.motorCount = 1;
    c.deadband = 0.012f;
    c.deadbandRel = 0.005f;
    c.driftThreshold = 0.01f;
    c.freezeIntegral = true;
    c.cusumK = 0.006f;
    c.cusumH = 0.12f;
    c.cusumJumpMax = 300;
    c.cusumJumpFrac = 0.55f;
    c.cusumHoldMs = 2000;
    return c;
}

struct FlowStepTrace
{
    float lastFlow = 0.0f;
    int maxVirtualPwm = 0;
    bool unreachable = false;
    uint16_t cusumAlarms = 0;
    float minFlow = 1e9f;
    float maxFlow = -1e9f;
};

static FlowStepTrace run_flow_steps(DualMotorFlowController &controller, PumpPlantSim &plant,
                                    const FlowPidConfig &config, float target,
                                    uint32_t startMs, uint32_t durationMs, uint32_t &sampleId)
{
    FlowStepTrace trace;
    for (uint32_t t = startMs; t < startMs + durationMs; t += 10)
    {
        plant.step();
        if ((t % 360) != 0) continue; // cadencia AFM07 medida en hardware
        DualMotorFlowInput input;
        input.nowMs = t;
        input.targetFlow = target;
        input.measuredFlow = plant.reading();
        input.flowValid = true;
        input.flowFresh = true;
        input.sampleId = ++sampleId;
        input.maxPwm = 2047;
        const DualMotorFlowOutput out = controller.update(input, config);
        plant.command = out.motor0Pwm;
        trace.minFlow = fminf(trace.minFlow, plant.flow);
        trace.maxFlow = fmaxf(trace.maxFlow, plant.flow);
        if (out.virtualPwm > trace.maxVirtualPwm) trace.maxVirtualPwm = out.virtualPwm;
        trace.unreachable = out.smartStatus.unreachable;
        trace.cusumAlarms = out.smartStatus.cusumAlarms;
    }
    trace.lastFlow = plant.flow;
    return trace;
}

static void test_flow_cusum_recovers_from_pump_gain_loss()
{
    // Hardware: con -15 % de ganancia el lazo histórico quedó en -12 % tras
    // 69 s; el CUSUM debe devolver el flujo a ±1 % en pocos segundos.
    const FlowPidConfig config = dron_v2_proto_config();
    DualMotorFlowController controller;
    PumpPlantSim plant;
    plant.gainScale = 1.04f;
    uint32_t sampleId = 0;
    run_flow_steps(controller, plant, config, 5.0f, 0, 20000, sampleId);
    plant.gainScale = 1.04f * 0.85f;
    FlowStepTrace after = run_flow_steps(controller, plant, config, 5.0f, 20000, 15000, sampleId);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, after.lastFlow);
    TEST_ASSERT_TRUE(after.cusumAlarms >= 1);

    // Recuperación también en la dirección opuesta (filtro retirado).
    plant.gainScale = 1.04f;
    after = run_flow_steps(controller, plant, config, 5.0f, 35000, 15000, sampleId);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, after.lastFlow);
}

static void test_flow_cusum_disabled_keeps_legacy_behavior()
{
    const FlowPidConfig config = dron_v2_flow_config(true);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, config.cusumH);
    TEST_ASSERT_EQUAL_UINT8(2, config.motorCount);
    DualMotorFlowController controller;
    PumpPlantSim plant;
    uint32_t sampleId = 0;
    run_flow_steps(controller, plant, config, 5.0f, 0, 20000, sampleId);
    plant.gainScale = 0.85f;
    const FlowStepTrace after = run_flow_steps(controller, plant, config, 5.0f, 20000, 15000, sampleId);
    TEST_ASSERT_EQUAL_UINT16(0, after.cusumAlarms);
    // Documenta la limitación medida: el trim acotado no compensa la pérdida.
    TEST_ASSERT_TRUE(after.lastFlow < 4.8f);
}

static void test_flow_relative_deadband_corrects_small_offset()
{
    // La tabla subestima la ganancia ~4 % (medido): el enclave a 1 L/min
    // quedaba en 0,96 con zona muerta 0,08. El prototipo debe cerrar a ±2 %.
    DualMotorFlowController legacy, proto;
    PumpPlantSim plantLegacy, plantProto;
    plantLegacy.gainScale = plantProto.gainScale = 1.06f;
    uint32_t idA = 0, idB = 0;
    run_flow_steps(legacy, plantLegacy, dron_v2_flow_config(true), 1.0f, 0, 30000, idA);
    const FlowStepTrace p = run_flow_steps(proto, plantProto, dron_v2_proto_config(), 1.0f, 0, 30000, idB);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, p.lastFlow);
    TEST_ASSERT_TRUE(fabsf(p.lastFlow - 1.0f) <= fabsf(plantLegacy.flow - 1.0f) + 0.005f);
}

static void test_flow_single_motor_caps_virtual_pwm_and_flags_unreachable()
{
    // 8 L/min supera el máximo de la bomba (6,7 L/min medidos a 2047).
    DualMotorFlowController single, dual;
    PumpPlantSim plantA, plantB;
    uint32_t idA = 0, idB = 0;
    const FlowStepTrace a = run_flow_steps(single, plantA, dron_v2_proto_config(), 8.0f, 0, 30000, idA);
    TEST_ASSERT_TRUE(a.maxVirtualPwm <= 2047);
    TEST_ASSERT_TRUE(a.unreachable);

    // Dos bombas conserva el rango virtual 0..4094 histórico.
    const FlowStepTrace b = run_flow_steps(dual, plantB, dron_v2_flow_config(true), 8.0f, 0, 30000, idB);
    TEST_ASSERT_TRUE(b.maxVirtualPwm > 2047);

    FlowPidConfig bad = dron_v2_proto_config();
    bad.motorCount = 0;
    TEST_ASSERT_FALSE(FlowMotorController::validateConfig(bad, 2047));
}

static void test_flow_cusum_jump_damping_converges_without_limit_cycle()
{
    // Hardware: con el salto completo el lazo oscilaba +-2 % (periodo 5 s).
    // Con cusumJumpFrac el flujo debe asentar sin ciclo límite tras escalones
    // de ganancia en ambos sentidos.
    const FlowPidConfig config = dron_v2_proto_config();
    TEST_ASSERT_EQUAL_FLOAT(0.55f, config.cusumJumpFrac);
    DualMotorFlowController controller;
    PumpPlantSim plant;
    plant.gainScale = 1.04f;
    uint32_t sampleId = 0;
    run_flow_steps(controller, plant, config, 5.0f, 0, 20000, sampleId);
    uint32_t t = 20000;
    const float steps[] = {0.80f, 1.04f, 0.88f};
    for (float gain : steps)
    {
        plant.gainScale = gain;
        run_flow_steps(controller, plant, config, 5.0f, t, 30000, sampleId); // asentar
        t += 30000;
        const FlowStepTrace obs = run_flow_steps(controller, plant, config, 5.0f, t, 20000, sampleId);
        t += 20000;
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, obs.minFlow);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, obs.maxFlow);
    }
    // Rango de validación de la fracción.
    FlowPidConfig bad = config;
    bad.cusumJumpFrac = 0.1f;
    TEST_ASSERT_FALSE(FlowMotorController::validateConfig(bad, 2047));
}

static void test_flow_cusum_pauses_when_flow_near_zero()
{
    // Línea tapada: flujo ~0 no debe disparar saltos de centro hacia el PWM
    // máximo.
    const FlowPidConfig config = dron_v2_proto_config();
    DualMotorFlowController controller;
    PumpPlantSim plant;
    plant.gainScale = 1.04f;
    uint32_t sampleId = 0;
    run_flow_steps(controller, plant, config, 5.0f, 0, 20000, sampleId);
    run_flow_steps(controller, plant, config, 5.0f, 20000, 5000, sampleId);
    plant.gainScale = 0.01f; // caudal residual < 0,2 L/min
    // El transitorio de caída (flujo aún > 0,2) puede disparar alarmas; una vez
    // en flujo casi nulo no debe haber ninguna más.
    const FlowStepTrace settling = run_flow_steps(controller, plant, config, 5.0f, 25000, 4000, sampleId);
    TEST_ASSERT_TRUE(settling.lastFlow < 0.2f);
    const FlowStepTrace blocked = run_flow_steps(controller, plant, config, 5.0f, 29000, 20000, sampleId);
    TEST_ASSERT_TRUE(blocked.lastFlow < 0.2f);
    TEST_ASSERT_EQUAL_UINT16(settling.cusumAlarms, blocked.cusumAlarms);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_battery_2s_pct_follows_discharge_curve);
    RUN_TEST(test_battery_2s_voltage_uses_divider_ratio);
    RUN_TEST(test_rtc_parser_is_calendar_aware);
    RUN_TEST(test_session_is_epoch_only);
    RUN_TEST(test_flow_schedule_boundary);
    RUN_TEST(test_capture_switch_wait_timing_matches_runtime_contract);
    RUN_TEST(test_modem_http_contract_rejects_oversized_url_and_payload);
    RUN_TEST(test_plantower_parser_decodes_signed_temperature_and_humidity);
    RUN_TEST(test_gnss_parser_converts_coordinates_speed_and_satellites);
    RUN_TEST(test_gnss_parser_rejects_missing_fix);
    RUN_TEST(test_http_url_normalize_adds_path_and_reports_errors);
    RUN_TEST(test_http_url_host_and_ip_rewrite);
    RUN_TEST(test_http_url_ipv4_validation);
    RUN_TEST(test_at_response_registration_status);
    RUN_TEST(test_at_response_ip_address_detection);
    RUN_TEST(test_signal_quality_parses_csq_and_maps_bars);
    RUN_TEST(test_status_led_temperature_gradient_and_low_power_peak);
    RUN_TEST(test_status_led_profiles_select_power_mode);
    RUN_TEST(test_console_args_read_named_values);
    RUN_TEST(test_console_args_parse_pid_config_overrides_only_given_fields);
    RUN_TEST(test_console_args_format_duration);
    RUN_TEST(test_rtc_to_unix_calendar_boundaries);
    RUN_TEST(test_rs485_stats_state_transitions);
    RUN_TEST(test_rs485_stats_exceptions_and_timing);
    RUN_TEST(test_i2c_retry_policy_matches_historical_backoff);
    RUN_TEST(test_fs3000_conversion_boundaries);
    RUN_TEST(test_afm07_fresh_stale_contract);
    RUN_TEST(test_afm07_sample_at_millis_zero_and_wraparound);
    RUN_TEST(test_flow_volume_integrates_each_physical_sample);
    RUN_TEST(test_dron_pid_does_not_use_calibration_seed);
    RUN_TEST(test_anemometer_conversion_and_expiry);
    RUN_TEST(test_rs485_requests_and_response_validation);
    RUN_TEST(test_rs485_schedule_reserves_afm07);
    RUN_TEST(test_afm_safety_retries_before_blocking);
    RUN_TEST(test_capture_safety_blocks_missing_prerequisites);
    RUN_TEST(test_plantower_frame_and_checksum);
    RUN_TEST(test_capture_state_machine_actions);
    RUN_TEST(test_motor_calibration_model);
    RUN_TEST(test_thermal_protection_dto);
    RUN_TEST(test_battery_protocol_real_frame);
    RUN_TEST(test_battery_protocol_rejects_corruption);
    RUN_TEST(test_dual_flow_virtual_mapping_and_sensor_grace);
    RUN_TEST(test_zero_flow_requires_two_fresh_run_samples_after_kick);
    RUN_TEST(test_zero_flow_confirmation_resets_and_latches_until_reset);
    RUN_TEST(test_zero_flow_ignores_disabled_pwm_and_inactive_target);
    RUN_TEST(test_smart_flow_seeks_and_locks_center_on_crossing);
    RUN_TEST(test_smart_flow_rejects_startup_spike_and_locks_only_at_target);
    RUN_TEST(test_smart_flow_deadband_holds_exact_center_pwm);
    RUN_TEST(test_smart_flow_soft_trim_limits_excursion_and_step);
    RUN_TEST(test_smart_flow_sensitivity_scaling);
    RUN_TEST(test_smart_flow_recenter_on_sustained_drift);
    RUN_TEST(test_smart_flow_fluid_transition_on_target_change);
    RUN_TEST(test_smart_flow_uncenters_on_close_targets);
    RUN_TEST(test_feed_forward_table_inverse_and_validation);
    RUN_TEST(test_feed_forward_limits_start_kick_for_low_targets);
    RUN_TEST(test_feed_forward_approach_locks_fast_without_overshoot);
    RUN_TEST(test_feed_forward_low_target_starts_pump_without_zero_flow);
    RUN_TEST(test_feed_forward_approach_handles_target_changes);
    RUN_TEST(test_feed_forward_is_faster_than_ramp_seek_and_keeps_zero_flow_guard);
    RUN_TEST(test_flow_motor_controller_no_kick_on_running_target_change);
    RUN_TEST(test_dual_motor_multi_stage_seamless_pwm);
    RUN_TEST(test_flow_cusum_recovers_from_pump_gain_loss);
    RUN_TEST(test_flow_cusum_disabled_keeps_legacy_behavior);
    RUN_TEST(test_flow_relative_deadband_corrects_small_offset);
    RUN_TEST(test_flow_single_motor_caps_virtual_pwm_and_flags_unreachable);
    RUN_TEST(test_flow_cusum_jump_damping_converges_without_limit_cycle);
    RUN_TEST(test_flow_cusum_pauses_when_flow_near_zero);
    return UNITY_END();
}
