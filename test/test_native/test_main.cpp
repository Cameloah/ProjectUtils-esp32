// Host-side unit tests for the hardware-independent parts. Run with: pio test -e native

#include <unity.h>

#include <cstring>
#include <string>

#include "pu/ByteQueue.h"
#include "pu/Hostname.h"
#include "pu/RingLog.h"
#include "pu/Semver.h"

using pu::ByteQueue;
using pu::LogLevel;
using pu::RingLog;
using pu::Semver;

void setUp() {}
void tearDown() {}

// ---------- Semver ----------

static void testSemverParsesWithAndWithoutPrefix() {
    Semver version = Semver{0, 0, 0};
    TEST_ASSERT_TRUE(Semver::parse("v1.2.3", version));
    TEST_ASSERT_EQUAL_UINT16(1, version.major);
    TEST_ASSERT_EQUAL_UINT16(2, version.minor);
    TEST_ASSERT_EQUAL_UINT16(3, version.patch);
    TEST_ASSERT_TRUE(Semver::parse("10.20.300", version));
    TEST_ASSERT_EQUAL_UINT16(300, version.patch);
}

static void testSemverRejectsMalformedInput() {
    Semver version = Semver{7, 7, 7};
    const char* bad[] = {"",      "v",       "1",         "1.2",      "1.2.",      "1..3",
                         "a.b.c", "1.2.3.4", "1.2.3-rc1", "v1.2.3\n", "99999.0.0", " 1.2.3"};
    for (const char* text : bad) {
        TEST_ASSERT_FALSE_MESSAGE(Semver::parse(text, version), text);
    }
    TEST_ASSERT_FALSE(Semver::parse(nullptr, version));
    // a failed parse must leave the output untouched
    TEST_ASSERT_EQUAL_UINT16(7, version.major);
}

static void testSemverOrdering() {
    TEST_ASSERT_TRUE((Semver{1, 0, 0}) > (Semver{0, 9, 9}));
    TEST_ASSERT_TRUE((Semver{1, 2, 0}) > (Semver{1, 1, 9}));
    TEST_ASSERT_TRUE((Semver{1, 2, 3}) > (Semver{1, 2, 2}));
    TEST_ASSERT_TRUE((Semver{1, 2, 3}) == (Semver{1, 2, 3}));
    TEST_ASSERT_FALSE((Semver{1, 2, 3}) > (Semver{1, 2, 3}));
    // numeric, not lexical: 0.10.0 is newer than 0.9.0, and values above 255 work
    TEST_ASSERT_TRUE((Semver{0, 10, 0}) > (Semver{0, 9, 0}));
    TEST_ASSERT_TRUE((Semver{0, 0, 300}) > (Semver{0, 0, 44}));
}

static void testSemverFormat() {
    char buffer[24];
    (Semver{1, 20, 300}).format(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("v1.20.300", buffer);
}

// ---------- Hostname ----------

static void testHostname() {
    TEST_ASSERT_EQUAL_STRING("myesp32abc123", pu::makeHostname("My ESP32 ABC123").c_str());
    TEST_ASSERT_EQUAL_STRING("living-room", pu::makeHostname("Living-Room").c_str());
    TEST_ASSERT_EQUAL_STRING("kche", pu::makeHostname("K\xC3\xBC"
                                                      "che!")
                                         .c_str());
    TEST_ASSERT_EQUAL_STRING("a-b", pu::makeHostname("-a-b-").c_str());
    TEST_ASSERT_EQUAL_STRING("esp32", pu::makeHostname("  !!  ").c_str());
    TEST_ASSERT_EQUAL_STRING("esp32", pu::makeHostname(nullptr).c_str());
    const std::string longName(100, 'x');
    TEST_ASSERT_EQUAL_UINT(32, pu::makeHostname(longName.c_str()).size());
}

// ---------- RingLog ----------

static void testRingLogKeepsOrderBeforeWrap() {
    RingLog<4> log;
    TEST_ASSERT_EQUAL_UINT(0, log.size());
    log.push(LogLevel::Info, 10, "a");
    log.push(LogLevel::Error, 20, "b");
    TEST_ASSERT_EQUAL_UINT(2, log.size());
    TEST_ASSERT_EQUAL_STRING("a", log.at(0).text);
    TEST_ASSERT_EQUAL_STRING("b", log.at(1).text);
    TEST_ASSERT_TRUE(log.at(1).level == LogLevel::Error);
    TEST_ASSERT_EQUAL_INT64(20, log.at(1).uptimeUs);
}

static void testRingLogWrapsAtCapacity() {
    RingLog<30> log;
    char text[8];
    // exactly full: nothing dropped, size never exceeds capacity
    for (int i = 0; i < 30; ++i) {
        snprintf(text, sizeof(text), "%d", i);
        log.push(LogLevel::Info, i, text);
    }
    TEST_ASSERT_EQUAL_UINT(30, log.size());
    TEST_ASSERT_EQUAL_STRING("0", log.at(0).text);
    TEST_ASSERT_EQUAL_STRING("29", log.at(29).text);
    // one more: oldest entry is replaced, order stays chronological
    log.push(LogLevel::Info, 30, "30");
    TEST_ASSERT_EQUAL_UINT(30, log.size());
    TEST_ASSERT_EQUAL_STRING("1", log.at(0).text);
    TEST_ASSERT_EQUAL_STRING("30", log.at(29).text);
    // several full rounds later
    for (int i = 31; i < 100; ++i) {
        snprintf(text, sizeof(text), "%d", i);
        log.push(LogLevel::Info, i, text);
    }
    TEST_ASSERT_EQUAL_UINT(30, log.size());
    TEST_ASSERT_EQUAL_STRING("70", log.at(0).text);
    TEST_ASSERT_EQUAL_STRING("99", log.at(29).text);
    log.clear();
    TEST_ASSERT_EQUAL_UINT(0, log.size());
}

static void testRingLogTruncatesLongText() {
    RingLog<2> log;
    const std::string longText(500, 'x');
    log.push(LogLevel::Warning, 0, longText.c_str());
    TEST_ASSERT_EQUAL_UINT(pu::kLogTextSize - 1, strlen(log.at(0).text));
    log.push(LogLevel::Warning, 0, nullptr);
    TEST_ASSERT_EQUAL_STRING("", log.at(1).text);
}

// ---------- ByteQueue ----------

static void testByteQueueFifo() {
    ByteQueue<4> queue;
    TEST_ASSERT_EQUAL_INT(-1, queue.pop());
    TEST_ASSERT_EQUAL_INT(-1, queue.peek());
    const uint8_t data[] = {1, 2, 3};
    TEST_ASSERT_EQUAL_UINT(3, queue.push(data, 3));
    TEST_ASSERT_EQUAL_INT(1, queue.peek());
    TEST_ASSERT_EQUAL_INT(1, queue.pop());
    TEST_ASSERT_EQUAL_INT(2, queue.pop());
    TEST_ASSERT_EQUAL_UINT(1, queue.size());
}

static void testByteQueueDropsOverflowAndWraps() {
    ByteQueue<4> queue;
    const uint8_t data[] = {10, 11, 12, 13, 14, 15};
    TEST_ASSERT_EQUAL_UINT(4, queue.push(data, 6)); // only 4 fit
    TEST_ASSERT_EQUAL_UINT(0, queue.push(data, 1)); // full
    TEST_ASSERT_EQUAL_INT(10, queue.pop());
    TEST_ASSERT_EQUAL_INT(11, queue.pop());
    TEST_ASSERT_EQUAL_UINT(2, queue.push(data + 4, 2)); // wraps around the end of the buffer
    TEST_ASSERT_EQUAL_INT(12, queue.pop());
    TEST_ASSERT_EQUAL_INT(13, queue.pop());
    TEST_ASSERT_EQUAL_INT(14, queue.pop());
    TEST_ASSERT_EQUAL_INT(15, queue.pop());
    TEST_ASSERT_EQUAL_INT(-1, queue.pop());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testSemverParsesWithAndWithoutPrefix);
    RUN_TEST(testSemverRejectsMalformedInput);
    RUN_TEST(testSemverOrdering);
    RUN_TEST(testSemverFormat);
    RUN_TEST(testHostname);
    RUN_TEST(testRingLogKeepsOrderBeforeWrap);
    RUN_TEST(testRingLogWrapsAtCapacity);
    RUN_TEST(testRingLogTruncatesLongText);
    RUN_TEST(testByteQueueFifo);
    RUN_TEST(testByteQueueDropsOverflowAndWraps);
    return UNITY_END();
}
