/**
 * @file test_fan_control.cpp
 * @brief Unit tests for the fan_control module
 */
#include <unity.h>
#include "fan_control.h" // Functions to test
#include "config.h"      // For global variables used by fan_control

// --- Global Variable Definitions (from config.h, needed for linking tests) ---
// These are normally in main.cpp, but for unit testing fan_control.cpp directly,
// we need to provide definitions or mock them.

// Modes & States
volatile bool isAutoMode = true;
volatile int manualFanSpeedPercentage = 50;
volatile bool isInMenuMode = false;
volatile bool isWiFiEnabled = false;
volatile bool serialDebugEnabled = false; // Set to true to see debug prints from setDefaultFanCurve
volatile float currentTemperature = 25.0;
volatile bool tempSensorFound = true;
volatile int fanRpm = 0;
volatile int fanSpeedPercentage = 0;
volatile int fanSpeedPWM_Raw = 0;
volatile unsigned long pulseCount = 0;
unsigned long lastRpmReadTime_Task = 0;

// Menu System Variables (not directly used by fan_control functions being tested, but part of config.h)
volatile MenuScreen currentMenuScreen = MAIN_MENU;
volatile int selectedMenuItem = 0;
volatile int scanResultCount = 0;
String scannedSSIDs[10];
char passwordInputBuffer[64] = "";
volatile int passwordCharIndex = 0;
volatile char currentPasswordEditChar = 'a';

// Fan Curve (Crucial for fan_control tests)
// MAX_CURVE_POINTS is defined in config.h (const int MAX_CURVE_POINTS = 8;)
int tempPoints[MAX_CURVE_POINTS];
int pwmPercentagePoints[MAX_CURVE_POINTS];
int numCurvePoints = 0;

// Staging Fan Curve (not used in functions being tested here)
int stagingTempPoints[MAX_CURVE_POINTS];
int stagingPwmPercentagePoints[MAX_CURVE_POINTS];
int stagingNumCurvePoints = 0;

// Task Communication
volatile bool needsImmediateBroadcast = false;
volatile bool rebootNeeded = false;

// Global Objects (Mocked or minimal definition if needed by tested functions)
// For the functions in fan_control.cpp, these specific objects are not directly used.
// If they were, proper mocking would be required.
Preferences preferences; // Mocked by default constructor
Adafruit_BMP280 bmp;     // Mocked by default constructor
LiquidCrystal_I2C lcd(0x27, 16, 2); // Mocked

// For ledcWrite and other Arduino hardware functions, they would either need to be
// mocked if running natively, or this test would run on the target.
// Unity tests can be run on the host (native) or on the target.
// For simplicity, we assume setFanSpeed's hardware interaction (ledcWrite)
// isn't the primary focus of these unit tests, but calculateAutoFanPWMPercentage is.

// Button Debouncing (not used by fan_control functions being tested)
unsigned long buttonPressTime[5];
bool buttonPressedState[5];
const long debounceDelay = 50;
const long longPressDelay = 1000;

// SSID and Password (not used by fan_control functions being tested)
char current_ssid[64] = "YOUR_WIFI_SSID";
char current_password[64] = "YOUR_WIFI_PASSWORD";

// --- Mock Arduino Functions (if needed for native testing) ---
// For native testing, some Arduino specific functions might need to be mocked.
// PlatformIO's native test runner often provides stubs for common ones.
// ledcWrite is a hardware-specific function. We'll provide a simple mock for it
// to allow setFanSpeed to be called without crashing in a native test environment.
uint32_t mock_ledcWrite_channel = 0;
uint32_t mock_ledcWrite_duty = 0;
void ledcWrite(uint8_t chan, uint32_t duty) {
    mock_ledcWrite_channel = chan;
    mock_ledcWrite_duty = duty;
    // You could add TEST_ASSERT_EQUAL_UINT8 here if you want to check the channel.
}

// --- Test Setup and Teardown ---
void setUp(void) {
    // set stuff up here
    // Reset mock values or global states before each test
    numCurvePoints = 0; // Clear any existing curve
    tempSensorFound = true; // Default to sensor found
    isAutoMode = true; // Default to auto mode
    mock_ledcWrite_channel = 0;
    mock_ledcWrite_duty = 0;
    fanSpeedPercentage = 0; // Reset global state potentially affected by setFanSpeed
    fanSpeedPWM_Raw = 0;    // Reset global state
    needsImmediateBroadcast = false;
}

void tearDown(void) {
    // clean stuff up here
}

// --- Test Cases for setDefaultFanCurve ---
void test_setDefaultFanCurve_values(void) {
    setDefaultFanCurve();
    TEST_ASSERT_EQUAL_INT(5, numCurvePoints); // Expect 5 points in default curve
    TEST_ASSERT_EQUAL_INT(25, tempPoints[0]);
    TEST_ASSERT_EQUAL_INT(0, pwmPercentagePoints[0]);
    TEST_ASSERT_EQUAL_INT(35, tempPoints[1]);
    TEST_ASSERT_EQUAL_INT(20, pwmPercentagePoints[1]);
    TEST_ASSERT_EQUAL_INT(45, tempPoints[2]);
    TEST_ASSERT_EQUAL_INT(50, pwmPercentagePoints[2]);
    TEST_ASSERT_EQUAL_INT(55, tempPoints[3]);
    TEST_ASSERT_EQUAL_INT(80, pwmPercentagePoints[3]);
    TEST_ASSERT_EQUAL_INT(60, tempPoints[4]);
    TEST_ASSERT_EQUAL_INT(100, pwmPercentagePoints[4]);
}

// --- Test Cases for calculateAutoFanPWMPercentage ---
void test_calculate_pwm_no_sensor(void) {
    tempSensorFound = false;
    setDefaultFanCurve(); // Have some curve data
    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE, pwm);
}

void test_calculate_pwm_no_curve_points(void) {
    tempSensorFound = true;
    numCurvePoints = 0; // Explicitly no points
    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(0, pwm); // Expect 0 if no curve points
}

void test_calculate_pwm_temp_below_first_point(void) {
    setDefaultFanCurve(); // Uses 25C as the first temp point with 0% PWM
    int pwm = calculateAutoFanPWMPercentage(20.0f); // Temp below 25C
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[0], pwm); // Should be PWM of the first point (0%)
}

void test_calculate_pwm_temp_above_last_point(void) {
    setDefaultFanCurve(); // Uses 60C as the last temp point with 100% PWM
    int pwm = calculateAutoFanPWMPercentage(70.0f); // Temp above 60C
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[numCurvePoints - 1], pwm); // Should be PWM of the last point (100%)
}

void test_calculate_pwm_temp_exactly_on_point(void) {
    setDefaultFanCurve();
    // Test on point 1: tempPoints[1] = 35, pwmPercentagePoints[1] = 20
    int pwm = calculateAutoFanPWMPercentage((float)tempPoints[1]);
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[1], pwm);

    // Test on point 2: tempPoints[2] = 45, pwmPercentagePoints[2] = 50
    pwm = calculateAutoFanPWMPercentage((float)tempPoints[2]);
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[2], pwm);
}

void test_calculate_pwm_temp_between_points_linear_interpolation(void) {
    setDefaultFanCurve();
    // Between point 1 (35C, 20%) and point 2 (45C, 50%)
    // Midpoint temperature: 40C
    // Expected PWM: 20 + ((40-35)/(45-35)) * (50-20) = 20 + (5/10)*30 = 20 + 0.5*30 = 20 + 15 = 35
    int pwm = calculateAutoFanPWMPercentage(40.0f);
    TEST_ASSERT_EQUAL_INT(35, pwm);

    // Another point: 37C
    // Expected PWM: 20 + ((37-35)/(45-35)) * (50-20) = 20 + (2/10)*30 = 20 + 0.2*30 = 20 + 6 = 26
    pwm = calculateAutoFanPWMPercentage(37.0f);
    TEST_ASSERT_EQUAL_INT(26, pwm);
}

void test_calculate_pwm_with_flat_segment_in_curve(void) {
    // Custom curve with a flat segment
    numCurvePoints = 3;
    tempPoints[0] = 30; pwmPercentagePoints[0] = 20;
    tempPoints[1] = 40; pwmPercentagePoints[1] = 50; // Point for interpolation
    tempPoints[2] = 40; pwmPercentagePoints[2] = 50; // tempRange = 0, should return pwmPercentagePoints[i]

    // Test case where temp is between tempPoints[i] and tempPoints[i+1],
    // but tempPoints[i+1] - tempPoints[i] (tempRange) is 0.
    // The function should return pwmPercentagePoints[i] for the segment where tempRange is 0.
    // This scenario implies that if temp is tempPoints[1] (40C), it should get pwmPercentagePoints[1] (50%).
    // The loop structure `temp >= tempPoints[i] && temp < tempPoints[i+1]` means
    // if temp == tempPoints[i+1], it will be caught by the next iteration or by the >= last point check.
    // Let's test temp at tempPoints[0]
    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(20, pwm);

    // This specific case (tempRange <= 0) should return pwmPercentagePoints[i]
    // If temp is 40, it should be handled by temp >= tempPoints[numCurvePoints - 1] (if 40 is the last temp)
    // or by the loop.
    // If the curve is: P0(20,10), P1(30,20), P2(30,30) (flat temp, rising pwm - invalid curve for the code logic)
    // The code expects tempPoints to be strictly increasing for interpolation.
    // Let's redefine the custom curve to be valid according to `saveFanCurveToNVS` validation
    // (i > 0 && tempTempPoints[i] <= tempTempPoints[i-1]) would make P2(40,50) invalid if P1 was (40,X)
    // So the current implementation's "if (tempRange <= 0) return pwmPercentagePoints[i];"
    // will handle cases where two consecutive temp points are identical.
    numCurvePoints = 4;
    tempPoints[0] = 20; pwmPercentagePoints[0] = 10;
    tempPoints[1] = 30; pwmPercentagePoints[1] = 40;
    tempPoints[2] = 30; pwmPercentagePoints[2] = 60; // tempPoints[1] == tempPoints[2]
    tempPoints[3] = 40; pwmPercentagePoints[3] = 80;

    // For the segment tempPoints[1] to tempPoints[2], tempRange is 0.
    // If currentTemperature is 30 (tempPoints[1]), it falls into this segment.
    // calculateAutoFanPWMPercentage(30.0f) will match i=1 (tempPoints[1]=30, tempPoints[2]=30)
    // tempRange = 0, so it should return pwmPercentagePoints[1] which is 40.
    pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(40, pwm); // pwmPercentagePoints[1]
}


// --- Test Cases for setFanSpeed ---
void test_setFanSpeed_updates_globals_and_calls_ledcWrite(void) {
    setFanSpeed(75); // Set to 75%
    TEST_ASSERT_EQUAL_INT(75, fanSpeedPercentage);
    // PWM_RESOLUTION_BITS is 8. Max duty is (1 << 8) - 1 = 255.
    // Expected raw PWM: map(75, 0, 100, 0, 255) = (75 * 255) / 100 = 191.25 -> 191 (integer map)
    // map function: long map(long x, long in_min, long in_max, long out_min, long out_max) {
    // return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }
    // (75 - 0) * (255 - 0) / (100 - 0) + 0 = 75 * 255 / 100 = 19125 / 100 = 191
    TEST_ASSERT_EQUAL_INT(191, fanSpeedPWM_Raw);
    TEST_ASSERT_TRUE(needsImmediateBroadcast);

    // Check if ledcWrite mock was called correctly
    TEST_ASSERT_EQUAL_UINT32(PWM_CHANNEL, mock_ledcWrite_channel); // PWM_CHANNEL = 0
    TEST_ASSERT_EQUAL_UINT32(191, mock_ledcWrite_duty);
}

void test_setFanSpeed_clamps_percentage_low(void) {
    setFanSpeed(-10); // Below 0
    TEST_ASSERT_EQUAL_INT(0, fanSpeedPercentage); // Should be clamped to 0
    TEST_ASSERT_EQUAL_INT(0, fanSpeedPWM_Raw);    // map(0, 0, 100, 0, 255) = 0
    TEST_ASSERT_TRUE(needsImmediateBroadcast);
    TEST_ASSERT_EQUAL_UINT32(0, mock_ledcWrite_duty);
}

void test_setFanSpeed_clamps_percentage_high(void) {
    setFanSpeed(110); // Above 100
    TEST_ASSERT_EQUAL_INT(100, fanSpeedPercentage); // Should be clamped to 100
    TEST_ASSERT_EQUAL_INT(255, fanSpeedPWM_Raw);   // map(100, 0, 100, 0, 255) = 255
    TEST_ASSERT_TRUE(needsImmediateBroadcast);
    TEST_ASSERT_EQUAL_UINT32(255, mock_ledcWrite_duty);
}


// --- Main for Test Runner ---
// IMPORTANT: If you are using PlatformIO's native test runner,
// `setup()` and `loop()` in this file might conflict with Arduino's `main.cpp`'s
// `setup()` and `loop()` if they are linked together.
// For unit tests, it's common to have a `main` function that calls Unity's runners.
// PlatformIO handles this when you run `pio test`.
// If `main.cpp` is not included in the test build (typical for unit tests focusing on a module):
#if defined(ARDUINO) && defined(UNIT_TEST)
// This setup/loop is for running on device if needed, or can be empty.
// For native tests, these are not strictly necessary if PlatformIO generates its own main.
void setup() {
    // Delay for a moment to allow the serial port to connect.
    delay(2000);
    UNITY_BEGIN();

    // Run tests for setDefaultFanCurve
    RUN_TEST(test_setDefaultFanCurve_values);

    // Run tests for calculateAutoFanPWMPercentage
    RUN_TEST(test_calculate_pwm_no_sensor);
    RUN_TEST(test_calculate_pwm_no_curve_points);
    RUN_TEST(test_calculate_pwm_temp_below_first_point);
    RUN_TEST(test_calculate_pwm_temp_above_last_point);
    RUN_TEST(test_calculate_pwm_temp_exactly_on_point);
    RUN_TEST(test_calculate_pwm_temp_between_points_linear_interpolation);
    RUN_TEST(test_calculate_pwm_with_flat_segment_in_curve);

    // Run tests for setFanSpeed
    RUN_TEST(test_setFanSpeed_updates_globals_and_calls_ledcWrite);
    RUN_TEST(test_setFanSpeed_clamps_percentage_low);
    RUN_TEST(test_setFanSpeed_clamps_percentage_high);

    UNITY_END();
}

void loop() {
    // Optional: can be empty or used for continuous testing scenarios
}
#else
// For native testing, PlatformIO usually provides its own main execution wrapper
// that calls the test functions. You just need to define the tests.
// This main is a fallback or for direct native compilation without PIO's test runner.
int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Run tests for setDefaultFanCurve
    RUN_TEST(test_setDefaultFanCurve_values);

    // Run tests for calculateAutoFanPWMPercentage
    RUN_TEST(test_calculate_pwm_no_sensor);
    RUN_TEST(test_calculate_pwm_no_curve_points);
    RUN_TEST(test_calculate_pwm_temp_below_first_point);
    RUN_TEST(test_calculate_pwm_temp_above_last_point);
    RUN_TEST(test_calculate_pwm_temp_exactly_on_point);
    RUN_TEST(test_calculate_pwm_temp_between_points_linear_interpolation);
    RUN_TEST(test_calculate_pwm_with_flat_segment_in_curve);

    // Run tests for setFanSpeed
    RUN_TEST(test_setFanSpeed_updates_globals_and_calls_ledcWrite);
    RUN_TEST(test_setFanSpeed_clamps_percentage_low);
    RUN_TEST(test_setFanSpeed_clamps_percentage_high);
    
    return UNITY_END();
}
#endif