#include <unity.h>
#include <math.h>
#include <string.h>
#include <Eolo/Core/Capture/CaptureControllerModel.h>
#include <Eolo/Core/Calibration/MotorCalibrationModel.h>
#include <Eolo/Core/Flow/FlowSchedule.h>
#include <Eolo/Core/Flow/DualMotorFlowController.h>
#include <Eolo/Core/Input/CaptureSwitchLogic.h>
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Core/Sensors/AnemometerModel.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>
#include <Eolo/Core/Sensors/NtcThermistor.h>
#include <Eolo/Core/Thermal/ThermalProtectionModel.h>
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include <Eolo/Core/Communication/GnssParser.h>
#include <Eolo/Core/Time/RtcTimeParser.h>
#include <Eolo/Types/HeadlessSetupTypes.h>
#include <Eolo/Core/Power/BatteryProtocol.h>
#include <Eolo/Core/Communication/RS485Protocol.h>
#include <Eolo/Types/ModemHttpContract.h>
#include "Board/I2CRetryPolicy.h"

static void test_rtc_parser_is_calendar_aware()
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
    TEST_ASSERT_TRUE(AFM07Model::refreshAge(data, lastSuccess, 2300, 1200, 15000));
    TEST_ASSERT_TRUE(data.stale);
    TEST_ASSERT_FALSE(AFM07Model::refreshAge(data, lastSuccess, 17001, 1200, 15000));
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
}

static void test_rs485_schedule_reserves_afm07()
{
    using Timing = EoloCore::RS485TimingModel;
    TEST_ASSERT_TRUE(Timing::kAnemometerSlotBudgetMs < Timing::kAfmIntervalMs);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1199, Timing::kAfmIntervalMs);
    TEST_ASSERT_TRUE(Timing::optionalFitsBeforeCritical(100, 300, true, 150));
    TEST_ASSERT_FALSE(Timing::optionalFitsBeforeCritical(100, 200, true, 150));
    TEST_ASSERT_TRUE(Timing::optionalFitsBeforeCritical(600, 100, true, 150));
    TEST_ASSERT_EQUAL_UINT32(1300, Timing::nextPeriodicDue(1000, 1150, 300));
    TEST_ASSERT_EQUAL_UINT32(0x2CU, Timing::nextPeriodicDue(0xFFFFFF00U, 0xFFFFFFF0U, 100));
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
    input.nowMs = 5999;
    output = controller.update(input, dual_flow_config());
    TEST_ASSERT_TRUE(output.sensorGraceActive);
    input.nowMs = 6001;
    output = controller.update(input, dual_flow_config());
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

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_rtc_parser_is_calendar_aware);
    RUN_TEST(test_session_is_epoch_only);
    RUN_TEST(test_flow_schedule_boundary);
    RUN_TEST(test_capture_switch_wait_timing_matches_runtime_contract);
    RUN_TEST(test_modem_http_contract_rejects_oversized_url_and_payload);
    RUN_TEST(test_plantower_parser_decodes_signed_temperature_and_humidity);
    RUN_TEST(test_gnss_parser_converts_coordinates_speed_and_satellites);
    RUN_TEST(test_gnss_parser_rejects_missing_fix);
    RUN_TEST(test_i2c_retry_policy_matches_historical_backoff);
    RUN_TEST(test_fs3000_conversion_boundaries);
    RUN_TEST(test_afm07_fresh_stale_contract);
    RUN_TEST(test_anemometer_conversion_and_expiry);
    RUN_TEST(test_rs485_requests_and_response_validation);
    RUN_TEST(test_rs485_schedule_reserves_afm07);
    RUN_TEST(test_plantower_frame_and_checksum);
    RUN_TEST(test_capture_state_machine_actions);
    RUN_TEST(test_motor_calibration_model);
    RUN_TEST(test_thermal_protection_dto);
    RUN_TEST(test_battery_protocol_real_frame);
    RUN_TEST(test_battery_protocol_rejects_corruption);
    RUN_TEST(test_dual_flow_virtual_mapping_and_sensor_grace);
    RUN_TEST(test_smart_flow_seeks_and_locks_center_on_crossing);
    RUN_TEST(test_smart_flow_rejects_startup_spike_and_locks_only_at_target);
    RUN_TEST(test_smart_flow_deadband_holds_exact_center_pwm);
    RUN_TEST(test_smart_flow_soft_trim_limits_excursion_and_step);
    RUN_TEST(test_smart_flow_sensitivity_scaling);
    RUN_TEST(test_smart_flow_recenter_on_sustained_drift);
    RUN_TEST(test_smart_flow_fluid_transition_on_target_change);
    RUN_TEST(test_smart_flow_uncenters_on_close_targets);
    RUN_TEST(test_flow_motor_controller_no_kick_on_running_target_change);
    RUN_TEST(test_dual_motor_multi_stage_seamless_pwm);
    return UNITY_END();
}
