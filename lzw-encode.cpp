#include <iostream>
#include <map>
#include <vector>

using namespace std;

struct lzw_encoder {
    void open() {
        initDict(true);
        appendCode(CLEAR_CODE);
        prefix.clear();
    }

    void append(uint8_t x) {
        prefix.push_back(x);
        if (dict.find(prefix) == dict.end())  { // no entry
            prefix.pop_back();
            appendCode(dict[prefix]);
            prefix.push_back(x);
            addTableEntry(prefix);
            prefix = {x};
        }
    }

    vector<uint8_t> close() {
        if (!prefix.empty()) {
            appendCode(dict[prefix]);
        }
        appendCode(EOI_CODE);
        return data;
    }
private:
    static constexpr uint16_t FIRST_RESTRICTED_CODE = 4094;
    static constexpr uint16_t CLEAR_CODE  = 256;
    static constexpr uint16_t EOI_CODE    = 257;

    size_t bitsWrote = 0;
    uint8_t cntBit = 8;
    vector<uint8_t> data;
    uint16_t currentCode = 0;
    map<vector<uint8_t>, uint16_t> dict;

    vector<uint8_t> prefix;

    [[nodiscard]] uint8_t getBitLen() const {
        return 32 - __builtin_clz(static_cast<uint32_t>(currentCode));
    }

    void initDict(bool firstInit = false) {
        if (firstInit) {
            bitsWrote = 0;
            data.clear();
        }
        dict.clear();
        for (size_t i = 0; i < 256; i++) {
            dict[{static_cast<uint8_t>(i)}] = static_cast<uint8_t>(i);
        }
        currentCode = 256 + 2;
        cntBit = getBitLen();
    }

    void addTableEntry(const vector<uint8_t> &entry) {
        dict[entry] = currentCode;
        currentCode++;
        cntBit = getBitLen();
        if (currentCode == FIRST_RESTRICTED_CODE) {
            //cerr << (int)cntBit << endl;
            appendCode(CLEAR_CODE);
            initDict();
        }
    }

    void appendCode(uint16_t code) {
        //cerr << hex << code << endl;
        for (uint8_t i = cntBit; i > 0; i--) {
            if ((bitsWrote & 7) == 0) {
                data.push_back(0);
            }
            uint16_t bit = (code >> (i - 1)) & 1;
            data.back() |= (bit << (7 - bitsWrote % 8));
            bitsWrote++;
        }
    }
};

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: lzw-encode <input file> <output file>" << endl;
        return 1;
    }
    if (freopen(argv[1], "r", stdin) == nullptr) {
        cerr << "Failed to open \"" << string(argv[1]) << "\" for reading" << endl;
        cerr << "Error code is " << errno << endl;
        return 1;
    }
    if (freopen(argv[2], "w", stdout) == nullptr) {
        cerr << "Failed to open \"" << string(argv[2]) << "\" for writing" << endl;
        cerr << "Error code is " << errno << endl;
        return 1;
    }
    lzw_encoder encoder;
    encoder.open();
    char c;
    while (cin.get(c)) {
        encoder.append(static_cast<uint8_t>(c));
    }
    auto result = encoder.close();
    cout.write(reinterpret_cast<const char *>(result.data()), result.size());
    if (cout.bad()) {
        cerr << "Failed to write result" << endl;
        cerr << "cout exceptions are " << cout.exceptions() << endl;
        return 1;
    }
    return 0;
}
