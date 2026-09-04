#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdlib>

namespace Astral::Test {

struct TestCaseResult {
    std::string suiteName;
    std::string testName;
    bool passed = true;
    std::string failureMessage;
    int line = 0;
    std::string file;
};

class TestRunner {
public:
    static TestRunner& Instance() {
        static TestRunner s_Instance;
        return s_Instance;
    }

    void RecordAssertion(const std::string& suiteName, const std::string& testName, bool condition,
                         const std::string& conditionStr, const std::string& message,
                         const char* file, int line) {
        m_TotalAssertions++;
        if (!condition) {
            TestCaseResult result;
            result.suiteName = suiteName;
            result.testName = testName;
            result.passed = false;
            result.failureMessage = message.empty() ? ("Assertion failed: " + conditionStr)
                                                   : ("Assertion failed: (" + conditionStr + ") - " + message);
            result.file = file;
            result.line = line;
            m_Failures.push_back(result);
            m_CurrentSuitePassed = false;
            std::cerr << "  [ FAILED ] " << suiteName << "::" << testName
                      << " at " << file << ":" << line << "\n"
                      << "             " << result.failureMessage << "\n";
        }
    }

    bool RunSuite(const std::string& suiteName, const std::function<void()>& suiteFunc) {
        std::cout << "[ RUN      ] " << suiteName << "\n";
        m_CurrentSuitePassed = true;
        auto start = std::chrono::high_resolution_clock::now();

        try {
            suiteFunc();
        } catch (const std::exception& e) {
            RecordAssertion(suiteName, "UnhandledException", false, "no exception",
                            std::string("Exception caught: ") + e.what(), __FILE__, __LINE__);
        } catch (...) {
            RecordAssertion(suiteName, "UnhandledException", false, "no exception",
                            "Unknown non-std exception caught", __FILE__, __LINE__);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        m_TotalSuites++;
        if (m_CurrentSuitePassed) {
            m_PassedSuites++;
            std::cout << "[       OK ] " << suiteName << " (" << elapsedMs << " ms)\n";
            return true;
        } else {
            std::cout << "[  FAILED  ] " << suiteName << " (" << elapsedMs << " ms)\n";
            return false;
        }
    }

    int PrintSummary() const {
        std::cout << "\n"
                  << "================================================================================\n";
        if (m_Failures.empty()) {
            std::cout << "[  PASSED  ] " << m_PassedSuites << "/" << m_TotalSuites
                      << " test suites passed successfully (" << m_TotalAssertions << " assertions verified).\n";
            std::cout << "================================================================================\n";
            return 0;
        } else {
            std::cerr << "[  FAILED  ] " << m_Failures.size() << " failure(s) in "
                      << (m_TotalSuites - m_PassedSuites) << " suite(s):\n";
            for (const auto& fail : m_Failures) {
                std::cerr << "  * " << fail.suiteName << "::" << fail.testName
                          << " (" << fail.file << ":" << fail.line << ")\n"
                          << "    " << fail.failureMessage << "\n";
            }
            std::cout << "================================================================================\n";
            return 1;
        }
    }

    void Reset() {
        m_TotalSuites = 0;
        m_PassedSuites = 0;
        m_TotalAssertions = 0;
        m_Failures.clear();
    }

private:
    TestRunner() = default;
    int m_TotalSuites = 0;
    int m_PassedSuites = 0;
    int m_TotalAssertions = 0;
    bool m_CurrentSuitePassed = true;
    std::vector<TestCaseResult> m_Failures;
};

} // namespace Astral::Test

#define TEST_CHECK_MSG(suite, test, cond, msg) \
    ::Astral::Test::TestRunner::Instance().RecordAssertion(suite, test, (cond), #cond, msg, __FILE__, __LINE__)

#define TEST_CHECK(suite, test, cond) \
    TEST_CHECK_MSG(suite, test, cond, "")
