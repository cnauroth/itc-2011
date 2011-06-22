// Chris Nauroth
// Intel Threading Challenge 2011
// Running Numbers

// ./runningnumbers 1BFC91544B9CBF9E5B93FFCAB7273070 38040301052B0163A103400502060501 05ED2F440000B17B0000000100000036

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>

#include <tbb/tick_count.h>

using namespace std;
using namespace tbb;

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

union buffercell {
    byte bufferbytes[4];
    word bufferword[2];
    dword bufferdword;
};

struct buffer {
    size_t size;
    buffercell* cells;

    bool isZero() const {
        bool result = true;

        for (size_t i = 0; i < this->size && result; ++i)
            result = (this->cells[i].bufferdword == 0x00000000);

        return result;
    }

    bool operator!=(const buffer& other) const {
        bool result = (this->size == other.size);

        if (result) {
            for (size_t i = 0; i < this->size && result; ++i)
                result = this->cells[i].bufferdword == other.cells[i].bufferdword;
        }

        return !result;
    }
};

buffer parseBuffer(const char* const in) {
    string str(in);
    size_t length = str.length();
    size_t size = length / 8;
    size += !!(length % 8);
    buffercell* cells = new buffercell[size];
    reverse(str.begin(), str.end());

    for (size_t i = 0; i < size; ++i) {
        string sub(str.substr(i * 8, 8));
        reverse(sub.begin(), sub.end());
        istringstream(sub) >> hex >> cells[size - i - 1].bufferdword;
    }

    buffer buf;
    buf.size = size;
    buf.cells = cells;
    return buf;
}

int main(int argc, char** argv) {
    tick_count begin = tick_count::now();

    if (argc < 4) {
        cerr << "Must specify source, byte increment, and dword increment."
             << endl;
        return 1;
    }

    buffer source(parseBuffer(argv[1]));
    buffer cycling(parseBuffer(argv[1]));
    buffer byteInc(parseBuffer(argv[2]));
    buffer dwordInc(parseBuffer(argv[3]));

    size_t i =0;
    for (; i == 0 || (source != cycling && !cycling.isZero()); ++i) {
        if (!(i % 37)) {
            for (size_t j = 0; j < cycling.size; ++j) {
                cycling.cells[j].bufferdword += dwordInc.cells[j].bufferdword;
            }
        }
        else {
            for (size_t j = 0; j < cycling.size; ++j) {
                for (size_t k = 0; k < 4; ++k) {
                    cycling.cells[j].bufferbytes[k] += byteInc.cells[j].bufferbytes[k];
                }
            }
        }

        /*
        cout << "Cycle " << dec << setw(4) << setfill('0') << i + 1 << ": ";

        for (size_t j = 0; j < cycling.size; ++j) {
            if (j > 0) cout << " ";
            cout << hex << setw(8) << setfill('0') << uppercase << cycling.cells[j].bufferdword;
        }

        cout << endl;
        */
    }

    cout << i << endl;
    cout << (tick_count::now() - begin).seconds() << endl;
    return 0;
}
