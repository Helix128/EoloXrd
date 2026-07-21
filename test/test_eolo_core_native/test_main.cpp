#include <unity.h>
#include <math.h>
#include <string.h>
#include <Eolo/Core/Capture/CaptureControllerModel.h>
#include <Eolo/Core/Calibration/MotorCalibrationModel.h>
#include <Eolo/Core/Flow/FlowSchedule.h>
#include <Eolo/Core/Sensors/AFM07Model.h>
#include <Eolo/Core/Sensors/AnemometerModel.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>
#include <Eolo/Core/Sensors/NtcThermistor.h>
#include <Eolo/Core/Thermal/ThermalProtectionModel.h>
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include <Eolo/Core/Time/RtcTimeParser.h>
#include <Eolo/Types/HeadlessSetupTypes.h>
#include <Eolo/Core/Power/BatteryProtocol.h>

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

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_rtc_parser_is_calendar_aware);
    RUN_TEST(test_session_is_epoch_only);
    RUN_TEST(test_flow_schedule_boundary);
    RUN_TEST(test_fs3000_conversion_boundaries);
    RUN_TEST(test_afm07_fresh_stale_contract);
    RUN_TEST(test_anemometer_conversion_and_expiry);
    RUN_TEST(test_plantower_frame_and_checksum);
    RUN_TEST(test_capture_state_machine_actions);
    RUN_TEST(test_motor_calibration_model);
    RUN_TEST(test_thermal_protection_dto);
    RUN_TEST(test_battery_protocol_real_frame);
    RUN_TEST(test_battery_protocol_rejects_corruption);
    return UNITY_END();
}
