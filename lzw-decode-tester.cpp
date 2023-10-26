#include <fstream>
#include <iostream>
#include <vector>
#include <functional>
#include "lzw-decoders.h"

using namespace std;

static __inline__ uint64_t rdtscp() {
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | (((uint64_t)hi) << 32);
}


struct test {
    test(const string &inputName, const string &outputName) {
        ifstream in(inputName, ios::binary);
        char c;
        while (in.get(c)) {
            data.push_back(static_cast<uint8_t>(c));
        }
        ifstream out(outputName, ios::binary);
        while (out.get(c)) {
            answer.push_back(static_cast<uint8_t>(c));
        }
    }

    explicit test(const string &name):
            test(name + ".in", name + ".out") {

    }

    [[nodiscard]] const vector<uint8_t>& getEncodedData() const {
        return data;
    }

    [[nodiscard]] bool checkDecodedData(const vector<uint8_t> &result, bool verbose = true) const {
        for (size_t i = 0; i < min(answer.size(), result.size()); i++) {
            if (result[i] != answer[i]) {
                if (verbose) {
                    cerr << "Difference 1.in byte " << i << endl;
                    cerr << "Expected: " << hex << static_cast<uint32_t>(answer[i]) << endl;
                    cerr << "Got: " << hex << static_cast<uint32_t>(result[i]) << endl;
                }
                return false;
            }
        }
        if (result.size() != answer.size()) {
            if (verbose) {
                cerr << "Answer and result have different length:" << endl;
                cerr << "Answer len: " << answer.size() << endl;
                cerr << "Result len: " << result.size() << endl;
            }
            return false;
        }
        return true;
    }

    [[nodiscard]] size_t getAnswerLen() const {
        return answer.size();
    }
private:
    vector<uint8_t> data;
    vector<uint8_t> answer;

};

using lzw_decoder = size_t(*)(const uint8_t*, size_t, uint8_t*, size_t);

// Get time(in some way) or -1, if test failed
uint64_t run_test(lzw_decoder decoder, const test &cntTest) {
    size_t ansLen = cntTest.getAnswerLen();
    // Add one guard element to detect overflow
    vector<uint8_t> outputData(ansLen + 1);

    uint64_t t1 = rdtscp();
    size_t wrote = decoder(cntTest.getEncodedData().data(), cntTest.getEncodedData().size(),
                           outputData.data(), outputData.size());
    uint64_t t2 = rdtscp();

    outputData.resize(wrote);
    if (cntTest.checkDecodedData(outputData)) {
        return t2 - t1;
    } else {
        return static_cast<uint64_t>(-1);
    }
}

vector<vector<uint64_t>> run_all(const vector<lzw_decoder> &runners, const vector<test> &tests, uint64_t samples = 10) {
    vector<vector<uint64_t>> results;
    int i = 0;
    for (auto runner : runners) {
        i++;
        results.emplace_back();
        cerr << "Tests for runner " << i << endl;
        int j = 0;
        for (auto &t : tests) {
            j++;
            cerr << "Test " << j << " started with " << samples << " samples" << endl;
            uint64_t sum = 0;
            for (int run = 0; run < samples; run++) {
                auto res = run_test(runner, t);
                if (res == static_cast<uint64_t>(-1)) {
                    sum = res;
                    break;
                }
                sum += res;
            }
            results.back().push_back(sum == ~0ULL ? -1 : (sum / samples));
            cerr << "Test " << j << " finished" << endl;
        }
    }
    return results;
}

int main() {
    vector<test> tests;
    vector<string> testNames;

    vector<lzw_decoder> runners;
    vector<string> runnerNames;

    function<void(const string&)> addTest = [&tests, &testNames](const string &name) {
        tests.emplace_back("./tests/" + name);
        testNames.push_back(name);
    };

    function<void(const string&, lzw_decoder)> addRunner =
            [&runners, &runnerNames](const string &name, lzw_decoder runner) {
        runners.push_back(runner);
        runnerNames.push_back(name);
    };

    addTest("empty");
    addTest("new-line");
    addTest("single-char");
    addTest("big-pnm");
    addTest("shell32.dll");
    addTest("1");
    addTest("2");
    addTest("3");
    addTest("4");

    addRunner("v0", decoderBasic);
    addRunner("v1", decoder1);
    addRunner("v1 fast read", decoder2);
    addRunner("v2", decoder3);

    auto result = run_all(runners, tests);

    for (int i = 0; i < runners.size(); i++) {
        cout << "Runner " << runnerNames[i] << ":\n";
        for (int j = 0; j < tests.size(); j++) {

            cout << "    Test " << testNames[j] << ": ";
            if (result[i][j] == ~0) {
                cout << "failed";
            } else {
                cout << result[i][j];
            }
            cout << "\n";
        }
    }
}
