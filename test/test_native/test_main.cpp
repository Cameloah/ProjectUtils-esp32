// Host-side unit tests for the hardware-independent parts. Run with: pio test -e native

#include <unity.h>

#include <cstring>
#include <string>

#include "pu/ApPolicy.h"
#include "pu/ByteQueue.h"
#include "pu/HoldState.h"
#include "pu/Hostname.h"
#include "pu/Ipv4.h"
#include "pu/LineQueue.h"
#include "pu/Numbers.h"
#include "pu/RingLog.h"
#include "pu/Semver.h"

using pu::ApMode;
using pu::ApPolicy;
using pu::ByteQueue;
using pu::HoldState;
using pu::LineQueue;
using pu::LogLevel;
using pu::NumberRange;
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

// ---------- LineQueue ----------

static size_t pushText(LineQueue<16>& queue, const char* text) {
    return queue.push(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

static void testLineQueueReturnsWholeLinesOnly() {
    LineQueue<16> queue;
    char line[16];
    pushText(queue, "abc");
    TEST_ASSERT_FALSE(queue.hasLine());
    TEST_ASSERT_EQUAL_INT(-1, queue.pop(line, sizeof(line)));
    pushText(queue, "def\r\n\nxy");
    TEST_ASSERT_EQUAL_INT(6, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("abcdef", line); // pieces joined, '\r' dropped
    TEST_ASSERT_EQUAL_INT(0, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("", line);                       // blank line preserved
    TEST_ASSERT_EQUAL_INT(-1, queue.pop(line, sizeof(line))); // "xy" is not finished yet
}

static void testLineQueueDropsNulAndSplitsLongLines() {
    LineQueue<16> queue;
    char line[8];
    const uint8_t withNul[] = {'h', 'i', '\0', '\n'};
    TEST_ASSERT_EQUAL_UINT(4, queue.push(withNul, sizeof(withNul)));
    TEST_ASSERT_EQUAL_INT(2, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("hi", line);

    // a line longer than the queue: the overflow is dropped, the rest comes out in pieces
    TEST_ASSERT_EQUAL_UINT(16, pushText(queue, "0123456789abcdefXYZ"));
    TEST_ASSERT_TRUE(queue.hasLine());
    TEST_ASSERT_EQUAL_INT(7, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("0123456", line);
    TEST_ASSERT_EQUAL_UINT(1, pushText(queue, "\n"));
    TEST_ASSERT_EQUAL_INT(7, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("789abcd", line);
    TEST_ASSERT_EQUAL_INT(2, queue.pop(line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("ef", line);
    TEST_ASSERT_EQUAL_UINT(0, queue.size());
}

// ---------- Numbers ----------

static void testParseNumberAcceptsOnlyCompleteFiniteNumbers() {
    double value = 7.0;
    TEST_ASSERT_TRUE(pu::parseNumber("12", value));
    TEST_ASSERT_EQUAL_DOUBLE(12.0, value);
    TEST_ASSERT_TRUE(pu::parseNumber("-0.5", value));
    TEST_ASSERT_EQUAL_DOUBLE(-0.5, value);
    TEST_ASSERT_TRUE(pu::parseNumber("1e3", value));
    TEST_ASSERT_EQUAL_DOUBLE(1000.0, value);

    value = 7.0;
    const char* bad[] = {"", " 1", "1 ", "abc", "1,5", "1.5x", "nan", "inf", "1e999", "--1"};
    for (const char* text : bad) {
        TEST_ASSERT_FALSE_MESSAGE(pu::parseNumber(text, value), text);
    }
    TEST_ASSERT_FALSE(pu::parseNumber(nullptr, value));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, value); // a failed parse leaves the output untouched
}

static void testWholeNumberAndRange() {
    TEST_ASSERT_TRUE(pu::isWholeNumber(0.0));
    TEST_ASSERT_TRUE(pu::isWholeNumber(-3.0));
    TEST_ASSERT_TRUE(pu::isWholeNumber(2147483647.0));
    TEST_ASSERT_FALSE(pu::isWholeNumber(0.5));
    TEST_ASSERT_FALSE(pu::isWholeNumber(2147483648.0)); // does not fit an int

    NumberRange range;
    TEST_ASSERT_TRUE(range.contains(-1e9)); // no bounds set
    range.hasMin = true;
    range.min = 0.1;
    TEST_ASSERT_TRUE(range.contains(0.1));
    TEST_ASSERT_FALSE(range.contains(0.0));
    TEST_ASSERT_TRUE(range.contains(1e9));
    range.hasMax = true;
    range.max = 60.0;
    TEST_ASSERT_TRUE(range.contains(60.0));
    TEST_ASSERT_FALSE(range.contains(60.5));
}

// ---------- HoldState ----------

static void testHoldStateReportsEdgesOnly() {
    HoldState hold;
    TEST_ASSERT_FALSE(hold.release());  // nothing held yet
    TEST_ASSERT_TRUE(hold.touch(100));  // press starts a hold
    TEST_ASSERT_FALSE(hold.touch(400)); // keep-alives do not start another one
    TEST_ASSERT_TRUE(hold.held());
    TEST_ASSERT_TRUE(hold.release());
    TEST_ASSERT_FALSE(hold.release());
    TEST_ASSERT_FALSE(hold.held());
}

static void testHoldStateExpiresWithoutKeepAlive() {
    HoldState hold;
    TEST_ASSERT_FALSE(hold.expire(5000, 1000)); // not held: nothing to expire
    hold.touch(1000);
    TEST_ASSERT_FALSE(hold.expire(1999, 1000));
    hold.touch(1900); // keep-alive pushes the deadline out
    TEST_ASSERT_FALSE(hold.expire(2899, 1000));
    TEST_ASSERT_TRUE(hold.expire(2900, 1000));
    TEST_ASSERT_FALSE(hold.held());
    TEST_ASSERT_FALSE(hold.expire(9999, 1000)); // reported once

    // across the rollover of the 32-bit millisecond counter
    hold.touch(0xFFFFFF00U);
    TEST_ASSERT_FALSE(hold.expire(0x00000010U, 1000)); // 272 ms later
    TEST_ASSERT_TRUE(hold.expire(0x00000300U, 1000));  // 1024 ms later
}

// ---------- ApPolicy ----------

using Action = ApPolicy::Action;

static ApPolicy makePolicy(ApMode mode, uint32_t timeoutMs, uint32_t graceMs) {
    ApPolicy::Options options;
    options.mode = mode;
    options.timeoutMs = timeoutMs;
    options.graceMs = graceMs;
    return ApPolicy(options);
}

static void testApPolicyOpensAtStart() {
    const ApMode modes[] = {ApMode::Never, ApMode::OnFailure, ApMode::Always};
    for (ApMode mode : modes) {
        const ApPolicy policy = makePolicy(mode, 0, 0);
        TEST_ASSERT_TRUE(policy.openAtStart(false, false)); // no WiFi stored: always set-up
    }
    TEST_ASSERT_FALSE(makePolicy(ApMode::Never, 0, 0).openAtStart(true, false));
    TEST_ASSERT_TRUE(makePolicy(ApMode::OnFailure, 0, 0).openAtStart(true, false));
    TEST_ASSERT_FALSE(makePolicy(ApMode::OnFailure, 0, 0).openAtStart(true, true));
    TEST_ASSERT_TRUE(makePolicy(ApMode::Always, 0, 0).openAtStart(true, true));
}

static void testApPolicyOpensAfterGracePeriodOnFailureOnly() {
    ApPolicy failure = makePolicy(ApMode::OnFailure, 0, 120000);
    TEST_ASSERT_EQUAL(Action::None, failure.update(1000, true, true, 0));
    TEST_ASSERT_EQUAL(Action::None, failure.update(2000, true, false, 0)); // lost
    TEST_ASSERT_EQUAL(Action::None, failure.update(120999, true, false, 0));
    TEST_ASSERT_EQUAL(Action::Open, failure.update(121000, true, false, 0));
    failure.opened(121000);
    TEST_ASSERT_EQUAL(Action::None, failure.update(200000, true, false, 0));
    TEST_ASSERT_EQUAL(Action::Close, failure.update(210000, true, true, 0)); // WiFi back
    failure.closed();
    TEST_ASSERT_EQUAL(Action::None, failure.update(220000, true, true, 0));

    // a short drop does not open it
    TEST_ASSERT_EQUAL(Action::None, failure.update(230000, true, false, 0));
    TEST_ASSERT_EQUAL(Action::None, failure.update(240000, true, true, 0));
    TEST_ASSERT_EQUAL(Action::None, failure.update(300000, true, false, 0));

    ApPolicy never = makePolicy(ApMode::Never, 0, 1000);
    never.update(0, true, true, 0);
    TEST_ASSERT_EQUAL(Action::None, never.update(999999, true, false, 0));
}

static void testApPolicyClientsKeepItOpen() {
    ApPolicy policy = makePolicy(ApMode::OnFailure, 60000, 0);
    policy.opened(0);
    TEST_ASSERT_EQUAL(Action::None, policy.update(1000, true, true, 1)); // WiFi back, but in use
    TEST_ASSERT_EQUAL(Action::None, policy.update(90000, false, false, 2));
    // the timeout counts from when the last client left
    TEST_ASSERT_EQUAL(Action::None, policy.update(149999, false, false, 0));
    TEST_ASSERT_EQUAL(Action::Close, policy.update(150000, false, false, 0));
}

static void testApPolicyTimeoutHoldsForTheRestOfTheOutage() {
    ApPolicy policy = makePolicy(ApMode::OnFailure, 60000, 1000);
    policy.opened(0); // opened at start-up, WiFi not reachable
    TEST_ASSERT_EQUAL(Action::Close, policy.update(60000, true, false, 0));
    policy.closed();
    TEST_ASSERT_EQUAL(Action::None, policy.update(500000, true, false, 0)); // still the same outage
    TEST_ASSERT_EQUAL(Action::None, policy.update(510000, true, true, 0));  // back
    TEST_ASSERT_EQUAL(Action::None, policy.update(510500, true, false, 0)); // lost again
    TEST_ASSERT_EQUAL(Action::Open, policy.update(511000, true, false, 0));
}

static void testApPolicyAlwaysClosesOnlyByTimeout() {
    ApPolicy policy = makePolicy(ApMode::Always, 0, 0);
    policy.opened(0);
    TEST_ASSERT_EQUAL(Action::None, policy.update(999999, true, true, 0));

    ApPolicy timed = makePolicy(ApMode::Always, 10000, 0);
    timed.opened(0);
    TEST_ASSERT_EQUAL(Action::Close, timed.update(10000, true, true, 0));
    timed.closed();
    TEST_ASSERT_EQUAL(Action::None, timed.update(20000, true, false, 0));
    TEST_ASSERT_EQUAL(Action::None, timed.update(999999, true, false, 0));
}

static void testApPolicyRollover() {
    ApPolicy policy = makePolicy(ApMode::OnFailure, 0, 1000);
    policy.update(0xFFFFFF00U, true, false, 0);
    policy.update(0xFFFFFF00U, true, true, 0);
    TEST_ASSERT_EQUAL(Action::None, policy.update(0x00000100U, true, false, 0)); // 512 ms lost
    TEST_ASSERT_EQUAL(Action::Open, policy.update(0x00000300U, true, false, 0)); // 1024 ms
}

// ---------- Ipv4 ----------

static void testIpv4ParsesOnlyDottedQuads() {
    uint32_t ip = 0;
    TEST_ASSERT_TRUE(pu::ipv4::parse("192.168.1.20", ip));
    TEST_ASSERT_EQUAL_HEX32(0xC0A80114U, ip);
    TEST_ASSERT_TRUE(pu::ipv4::parse("0.0.0.0", ip));
    TEST_ASSERT_TRUE(pu::ipv4::parse("255.255.255.255", ip));
    const char* bad[] = {"",       "1.2.3",    "1.2.3.4.5", "256.1.1.1",  "1.2.3.4 ", " 1.2.3.4",
                         "1..2.3", "01.2.3.4", "1.2.3.a",   "1.2.3.1000", "1.2.3.-4", nullptr};
    for (const char* text : bad) {
        TEST_ASSERT_FALSE_MESSAGE(pu::ipv4::parse(text, ip), text == nullptr ? "null" : text);
    }
}

static void testIpv4ChecksStaticConfiguration() {
    using pu::ipv4::StaticIpError;
    const auto check = [](const char* address, const char* mask, const char* gateway) {
        uint32_t a = 0;
        uint32_t m = 0;
        uint32_t g = 0;
        TEST_ASSERT_TRUE(pu::ipv4::parse(address, a) && pu::ipv4::parse(mask, m) &&
                         pu::ipv4::parse(gateway, g));
        return pu::ipv4::checkStatic(a, m, g);
    };
    TEST_ASSERT_EQUAL(StaticIpError::None, check("192.168.1.20", "255.255.255.0", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::None, check("10.0.5.7", "255.255.0.0", "10.0.0.1"));
    TEST_ASSERT_EQUAL(StaticIpError::None, check("192.168.1.2", "255.255.255.252", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::InvalidMask,
                      check("192.168.1.20", "255.0.255.0", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::InvalidMask, check("192.168.1.20", "0.0.0.0", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::InvalidMask,
                      check("192.168.1.20", "255.255.255.254", "192.168.1.21"));
    TEST_ASSERT_EQUAL(StaticIpError::AddressNotUsable,
                      check("192.168.1.0", "255.255.255.0", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::AddressNotUsable,
                      check("192.168.1.255", "255.255.255.0", "192.168.1.1"));
    TEST_ASSERT_EQUAL(StaticIpError::AddressNotUsable,
                      check("127.0.0.5", "255.0.0.0", "127.0.0.1"));
    TEST_ASSERT_EQUAL(StaticIpError::AddressNotUsable,
                      check("224.0.0.5", "255.255.255.0", "224.0.0.1"));
    TEST_ASSERT_EQUAL(StaticIpError::GatewayNotUsable,
                      check("192.168.1.20", "255.255.255.0", "192.168.1.255"));
    TEST_ASSERT_EQUAL(StaticIpError::GatewayOutsideSubnet,
                      check("192.168.1.20", "255.255.255.0", "192.168.0.1"));
    TEST_ASSERT_EQUAL(StaticIpError::GatewayIsAddress,
                      check("192.168.1.1", "255.255.255.0", "192.168.1.1"));
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
    RUN_TEST(testLineQueueReturnsWholeLinesOnly);
    RUN_TEST(testLineQueueDropsNulAndSplitsLongLines);
    RUN_TEST(testParseNumberAcceptsOnlyCompleteFiniteNumbers);
    RUN_TEST(testWholeNumberAndRange);
    RUN_TEST(testHoldStateReportsEdgesOnly);
    RUN_TEST(testHoldStateExpiresWithoutKeepAlive);
    RUN_TEST(testApPolicyOpensAtStart);
    RUN_TEST(testApPolicyOpensAfterGracePeriodOnFailureOnly);
    RUN_TEST(testApPolicyClientsKeepItOpen);
    RUN_TEST(testApPolicyTimeoutHoldsForTheRestOfTheOutage);
    RUN_TEST(testApPolicyAlwaysClosesOnlyByTimeout);
    RUN_TEST(testApPolicyRollover);
    RUN_TEST(testIpv4ParsesOnlyDottedQuads);
    RUN_TEST(testIpv4ChecksStaticConfiguration);
    return UNITY_END();
}
