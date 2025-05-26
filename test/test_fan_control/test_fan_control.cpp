/**
 * @file test_fan_control.cpp
 * @brief Unit tests for the fan_control module.
 * Relies on global variables being defined in src/main.cpp and linked
 * when test_build_src = true in platformio.ini.
 */
#include <unity.h>
#include "fan_control.h" // Functions to test
#include "config.h"      // For extern declarations of global variables from config.h
                         // config.h includes Arduino.h, Preferences.h, Adafruit_BMP280.h, LiquidCrystal_I2C.h etc.
                         // The actual objects (lcd, bmp, preferences) and global variables
                         // will be defined in main.cpp and linked.

// --- DO NOT REDEFINE GLOBAL VARIABLES OR ARDUINO CORE FUNCTIONS HERE ---
// They are now provided by src/main.cpp and tpio test -vvvhe Arduino framework
// because test_build_src = true.

// --- Test Setup and Teardown (called by Unity test runner) ---
void setUp(void) {
    // This function is called before each test case.
    // Initialize or reset the state of global variables (defined in main.cpp) here
    // if your tests require a specific starting state.
    // These variables are declared 'extern' in config.h and defined in main.cpp.
    numCurvePoints = 0;
    tempSensorFound = true;
    isAutoMode = true;
    fanSpeedPercentage = 0;
    fanSpeedPWM_Raw = 0;
    needsImmediateBroadcast = false;
    currentTemperature = 25.0f; // Default for tests if needed
    // Initialize other globals from main.cpp as needed for consistent test states
}

void tearDown(void) {
    // This function is called after each test case.
    // Clean up any resources if necessary.
}

// --- Test Cases for setDefaultFanCurve ---
void test_setDefaultFanCurve_values(void) {
    setDefaultFanCurve(); // This function is in fan_control.cpp
    TEST_ASSERT_EQUAL_INT(5, numCurvePoints);
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
    tempSensorFound = false; // Global variable from main.cpp
    setDefaultFanCurve();
    // AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE is a const int defined in main.cpp
    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE, pwm);
}

void test_calculate_pwm_no_curve_points(void) {
    tempSensorFound = true;
    numCurvePoints = 0; // Global variable from main.cpp
    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(0, pwm);
}

void test_calculate_pwm_temp_below_first_point(void) {
    setDefaultFanCurve();
    int pwm = calculateAutoFanPWMPercentage(20.0f);
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[0], pwm); // Global array from main.cpp
}

void test_calculate_pwm_temp_above_last_point(void) {
    setDefaultFanCurve();
    int pwm = calculateAutoFanPWMPercentage(70.0f);
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[numCurvePoints - 1], pwm);
}

void test_calculate_pwm_temp_exactly_on_point(void) {
    setDefaultFanCurve();
    int pwm = calculateAutoFanPWMPercentage((float)tempPoints[1]); // Global array from main.cpp
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[1], pwm);
    pwm = calculateAutoFanPWMPercentage((float)tempPoints[2]);
    TEST_ASSERT_EQUAL_INT(pwmPercentagePoints[2], pwm);
}

void test_calculate_pwm_temp_between_points_linear_interpolation(void) {
    setDefaultFanCurve();
    int pwm = calculateAutoFanPWMPercentage(40.0f);
    TEST_ASSERT_EQUAL_INT(35, pwm);
    pwm = calculateAutoFanPWMPercentage(37.0f);
    TEST_ASSERT_EQUAL_INT(26, pwm);
}

void test_calculate_pwm_with_flat_segment_in_curve(void) {
    // Setup a custom curve using global arrays
    numCurvePoints = 4;
    tempPoints[0] = 20; pwmPercentagePoints[0] = 10;
    tempPoints[1] = 30; pwmPercentagePoints[1] = 40;
    tempPoints[2] = 30; pwmPercentagePoints[2] = 60; // tempPoints[1] == tempPoints[2]
    tempPoints[3] = 40; pwmPercentagePoints[3] = 80;

    int pwm = calculateAutoFanPWMPercentage(30.0f);
    TEST_ASSERT_EQUAL_INT(40, pwm); // Expects pwmPercentagePoints[1] due to tempRange = 0 logic
}

// --- Test Cases for setFanSpeed ---
// Note: We cannot directly test if ledcWrite was called with specific parameters
// without more advanced mocking frameworks or running on target.
// We test the effect on global variables.
void test_setFanSpeed_updates_globals(void) {
    setFanSpeed(75); // This function is in fan_control.cpp
    TEST_ASSERT_EQUAL_INT(75, fanSpeedPercentage); // Global variable from main.cpp
    // PWM_RESOLUTION_BITS is a const int defined in main.cpp (via config.h)
    // Expected raw PWM: map(75, 0, 100, 0, (1 << PWM_RESOLUTION_BITS) - 1)
    // If PWM_RESOLUTION_BITS = 8, max_duty = 255. (75 * 255) / 100 = 191.25 -> 191
    TEST_ASSERT_EQUAL_INT(191, fanSpeedPWM_Raw); // Global variable from main.cpp
    TEST_ASSERT_TRUE(needsImmediateBroadcast); // Global variable from main.cpp
}

void test_setFanSpeed_clamps_percentage_low(void) {
    setFanSpeed(-10);
    TEST_ASSERT_EQUAL_INT(0, fanSpeedPercentage);
    TEST_ASSERT_EQUAL_INT(0, fanSpeedPWM_Raw);
    TEST_ASSERT_TRUE(needsImmediateBroadcast);
}

void test_setFanSpeed_clamps_percentage_high(void) {
    setFanSpeed(110);
    TEST_ASSERT_EQUAL_INT(100, fanSpeedPercentage);
    // If PWM_RESOLUTION_BITS = 8, max_duty = 255.
    TEST_ASSERT_EQUAL_INT(255, fanSpeedPWM_Raw);
    TEST_ASSERT_TRUE(needsImmediateBroadcast);
}

// --- Test Runner Invocation ---
// PlatformIO's test runner will manage how these Unity tests are executed
// when linked with the main application code (due to test_build_src = true).
// The standard Arduino setup() and loop() from src/main.cpp will be present.
// To run these tests, PlatformIO typically calls a function that executes
// UNITY_BEGIN(), then all RUN_TEST(...) macros, and finally UNITY_END().
// This might be integrated into the main loop or setup, or handled by
// PlatformIO's test runner hooks.

// If running on target, you might need to call this from your main setup()
// void run_tests() {
//     UNITY_BEGIN();
//     RUN_TEST(test_setDefaultFanCurve_values);
//     RUN_TEST(test_calculate_pwm_no_sensor);
//     RUN_TEST(test_calculate_pwm_no_curve_points);
//     RUN_TEST(test_calculate_pwm_temp_below_first_point);
//     RUN_TEST(test_calculate_pwm_temp_above_last_point);
//     RUN_TEST(test_calculate_pwm_temp_exactly_on_point);
//     RUN_TEST(test_calculate_pwm_temp_between_points_linear_interpolation);
//     RUN_TEST(test_calculate_pwm_with_flat_segment_in_curve);
//     RUN_TEST(test_setFanSpeed_updates_globals);
//     RUN_TEST(test_setFanSpeed_clamps_percentage_low);
//     RUN_TEST(test_setFanSpeed_clamps_percentage_high);
//     UNITY_END();
// }

// In src/main.cpp -> setup():
// #ifdef UNIT_TEST
//   delay(2000); // Give time for serial connection
//   run_tests();
// #endif

// However, often for host-based testing or when PlatformIO manages it,
// simply defining the setUp, tearDown, and test_ functions is sufficient.
// The UNITY_BEGIN, RUN_TEST, UNITY_END calls are implicitly handled or
// should be placed in a standard entry point if required by your specific
// test environment setup (e.g., if you were creating a separate main_test.cpp).
// For now, we assume PlatformIO's default behavior with test_build_src=true
// will correctly execute these.
// If tests don't run automatically, you'll need to add the explicit runner calls
// (UNITY_BEGIN, RUN_TEST, UNITY_END) in a function that gets called during test execution.
// A common way is to add this to the setup() in your main.cpp, guarded by #ifdef UNIT_TEST.

// For the ESP32 target, PlatformIO will typically define a main that calls setup() and loop().
// To integrate Unity tests, you often need to call the test runner from within setup().

// Let's add the explicit runner calls here, which PlatformIO's test system
// should pick up when building for the test environment.
#ifdef UNIT_TEST
void process_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_setDefaultFanCurve_values);
    RUN_TEST(test_calculate_pwm_no_sensor);
    RUN_TEST(test_calculate_pwm_no_curve_points);
    RUN_TEST(test_calculate_pwm_temp_below_first_point);
    RUN_TEST(test_calculate_pwm_temp_above_last_point);
    RUN_TEST(test_calculate_pwm_temp_exactly_on_point);
    RUN_TEST(test_calculate_pwm_temp_between_points_linear_interpolation);
    RUN_TEST(test_calculate_pwm_with_flat_segment_in_curve);
    RUN_TEST(test_setFanSpeed_updates_globals);
    RUN_TEST(test_setFanSpeed_clamps_percentage_low);
    RUN_TEST(test_setFanSpeed_clamps_percentage_high);
    UNITY_END();
}

// If running on the ESP32 target, this setup() and loop() will be called.
// The main application's setup() and loop() from src/main.cpp will also be present
// due to test_build_src=true. This can lead to issues if not handled.
// A common pattern is to have the main setup() in src/main.cpp call process_tests()
// when UNIT_TEST is defined.

// For tests run on the host via `pio test -e native` (if you had a native env),
// this setup/loop might not be used, and a different main would call process_tests().
// Given you're testing with the `upesy_wroom` env, PlatformIO will try to run this on host
// or target. If on host, it stubs Arduino functions.

// To avoid conflicts with src/main.cpp's setup/loop when test_build_src=true,
// it's often better to NOT have setup()/loop() in the test file itself,
// and instead call the test runner from src/main.cpp's setup() conditionally.

// However, let's try with a minimal setup/loop here specifically for the test runner,
// assuming PlatformIO might prioritize this when in a test context for the file.
// This is a bit of a gray area depending on exact PIO version and test runner behavior.

// The most robust way when test_build_src = true is:
// 1. No setup()/loop() in test_*.cpp files.
// 2. In src/main.cpp:
//    void setup() {
//        #ifdef UNIT_TEST
//            delay(2000); // For serial monitor
//            UNITY_BEGIN();
//            RUN_TEST(test_...); // from test_fan_control.cpp (needs to be callable)
//            // ... more RUN_TEST calls
//            UNITY_END();
//            // Optionally, stop further execution or loop indefinitely
//            while(1);
//        #else
//            // Normal setup code
//        #endif
//    }
//    void loop() {
//        #ifdef UNIT_TEST
//            // Do nothing or minimal processing
//        #else
//            // Normal loop code
//        #endif
//    }
// To make RUN_TEST callable from main.cpp, the test functions would need to be
// declared (e.g. in a test_runner.h) or the RUN_TEST calls placed in a function
// within the test file that is then called from main.cpp's setup.

// For simplicity and to see if PIO handles it, we'll put the runner calls
// in a function and then call it from a setup() here, guarded by UNIT_TEST.
// This might still conflict if PIO directly uses src/main.cpp's setup.

// Let's remove the setup/loop from here and rely on the test runner finding the tests.
// If it doesn't, the next step is to integrate the test calls into src/main.cpp's setup.

#endif // UNIT_TEST

// It's generally better to let PlatformIO's Unity integration discover and run
// the `test_*` functions automatically without needing a custom `setup` or `loop`
// in the test file itself when `test_build_src = true`.
// The `UNITY_BEGIN`, `RUN_TEST`, and `UNITY_END` calls are typically
// injected by the test runner framework.

// If direct execution is needed (e.g. for on-target testing without full runner integration)
// you would add something like this in your main.cpp (conditionally):
/*
 #ifdef UNIT_TEST
 void run_all_my_tests() {
    UNITY_BEGIN();
    // You'd need to declare or include headers for these test functions if called from main.cpp
    // For example, forward declare them or put RUN_TEST calls in a function within this file
    // that is then called from main.cpp
    // RUN_TEST(test_setDefaultFanCurve_values);
    // ...
    UNITY_END();
 }
 #endif

 // In setup() of main.cpp:
 // #ifdef UNIT_TEST
 //   delay(2000);
 //   run_all_my_tests();
 //   while(1) {} // Halt after tests
 // #else
 //   // normal setup
 // #endif
*/

// For now, the test file will only contain setUp, tearDown, and test_ functions.
// The Unity test runner should handle the rest.
