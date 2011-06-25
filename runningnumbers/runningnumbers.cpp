// Chris Nauroth
// Intel Threading Challenge 2011
// Running Numbers

// ./runningnumbers 1BFC91544B9CBF9E5B93FFCAB7273070 38040301052B0163A103400502060501 05ED2F440000B17B0000000100000036
// ./runningnumbers 6DDEFED46602FB0E9B671E1A05B1FE10 38040301052B0163A103400502060501 05ED2F440000B17B0000000100000036

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

    buffer& operator=(const buffer& other) {
        delete[] this->cells;
        this->size = other.size;
        this->cells = new buffercell[this->size];

        for (size_t i = 0; i < this->size; ++i)
            this->cells[i].bufferdword = other.cells[i].bufferdword;

        return *this;
    }

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
    buffer byteInc(parseBuffer(argv[2]));
    buffer dwordInc(parseBuffer(argv[3]));

    size_t i = 0;
    buffer cycling;
    cycling.size = 0;
    cycling.cells = NULL;
    for (; i == 0 || (source != cycling && !cycling.isZero()); ++i) {
        cycling = source;

        for (size_t j = 0; j <= i; j += 37) {
            for (size_t k = 0; k < cycling.size; ++k) {
                cycling.cells[k].bufferdword += dwordInc.cells[k].bufferdword;
            }

            size_t cycleOffset = i - j;
            size_t byteIncrements = (cycleOffset >= 36 ? 36 : cycleOffset);

            for (size_t k = 0; k < cycling.size; ++k) {
                cycling.cells[k].bufferbytes[0] +=
                    (byteInc.cells[k].bufferbytes[0] * byteIncrements);
                cycling.cells[k].bufferbytes[1] +=
                    (byteInc.cells[k].bufferbytes[1] * byteIncrements);
                cycling.cells[k].bufferbytes[2] +=
                    (byteInc.cells[k].bufferbytes[2] * byteIncrements);
                cycling.cells[k].bufferbytes[3] +=
                    (byteInc.cells[k].bufferbytes[3] * byteIncrements);
            }
        }
    }

    cout << dec << i << endl;
    cout << (tick_count::now() - begin).seconds() << endl;
    return 0;
}
