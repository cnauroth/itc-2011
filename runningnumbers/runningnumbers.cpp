// Chris Nauroth
// Intel Threading Challenge 2011
// Running Numbers

// ./runningnumbers 1BFC91544B9CBF9E5B93FFCAB7273070 38040301052B0163A103400502060501 05ED2F440000B17B0000000100000036
// 4774
// ./runningnumbers 6DDEFED46602FB0E9B671E1A05B1FE10 38040301052B0163A103400502060501 05ED2F440000B17B0000000100000036
// 574395734

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdint.h>
#include <vector>

#include <tbb/atomic.h>
#include <tbb/concurrent_vector.h>
#include <tbb/mutex.h>
#include <tbb/parallel_do.h>
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

    bool operator==(const buffer& other) const {
        return !(*this != other);
    }
};

static mutex coutMutex;
static atomic<size_t> _solution;

// TODO: make it a random-access iterator

class InputIterator {
public:
    typedef size_t difference_type;
    typedef input_iterator_tag iterator_category;
    typedef size_t* pointer;
    typedef size_t& reference;
    typedef size_t value_type;

    static InputIterator& end() {
        static InputIterator theEnd(true, 0);
        return theEnd;
    }

    InputIterator():_end(false), _i(0) {
    }

    InputIterator(const size_t i):_end(false), _i(i) {
    }

    InputIterator(const bool end, const size_t i):_end(end), _i(i) {
    }

    InputIterator(const InputIterator& other):_end(other._end), _i(other._i) {
    }

    InputIterator& operator=(const InputIterator& other) {
        this->_end = other._end;
        this->_i = other._i;
        return *this;
    }

    bool operator==(const InputIterator& other) const {
        //mutex::scoped_lock coutLock(coutMutex);
        bool result;

        /*
        if (this->_end) {
            cout << "this is at end" << endl;
            result = other._end || (this->_i >= _solution);
        }
        else {
            result = (!other._end && this->_i == other._i);
        }
        */

        /*
        if (this->_i >= _solution) {
            if (other._end) {
                result = true;
            }
            else {
                result = (this->_i == other._i);
            }
        }
        else {
            result = (this->_i == other._i);
        }
        */

        if (this->_end) {
            if (other._end) {
                //cout << "operator== A" << endl;
                result = true;
            }
            else {
                //cout << "operator== B" << endl;
                result = false;
            }
        }
        else if (other._end) {
            if (this->_end) {
                //cout << "operator== C" << endl;
                result = true;
            }
            else {
                //cout << "operator== D" << endl;
                if (this->_i >= _solution) {
                    //cout << "operator== D1, this->_i = " << dec << this->_i << ", _solution = " << _solution << endl;
                    result = true;
                }
                else {
                    //cout << "operator== D2" << endl;
                    result = false;
                }
            }
        }
        else {
            if (this->_i == other._i) {
                //cout << "operator== E" << endl;
                result = true;
            }
            else {
                //cout << "operator== F" << endl;
                result = false;
            }
        }

        //cout << "operator==" << result << endl;
        return result;
    }

    /*
    bool operator!=(const InputIterator& other) const {
        //cout << "operator!=" << endl;
        //return (this->_end != other._end || this->_i != other._i);
        return !(*this == other);
    }
    */

    InputIterator& operator++() {
        //cout << "operator++" << endl;
        //        mutex::scoped_lock coutLock2(coutMutex);
        //cout << "operator++ this->_i = " << dec << this ->_i << ", _solution = " << dec << _solution << endl;
        ++this->_i;
        if (this->_i >= _solution) {
            /*
            {
                mutex::scoped_lock coutLock(coutMutex);
                cout << "returning end()" << endl;
            }
            */
            return end();
        }
        else return *this;
    }

    size_t operator*() {
        //cout << "operator*" << endl;
        return this->_i;
    }
private:
    bool _end;
    size_t _i;
};

class Apply {
public:
    typedef size_t argument_type;

    Apply(const buffer& byteInc, const size_t cycleIncrement,
          const buffer& dwordInc, const buffer& source):
        _byteInc(byteInc), _cycleIncrement(cycleIncrement), _dwordInc(dwordInc),
        _source(source) {

        _solution = numeric_limits<size_t>::max();
    }

    void operator()(argument_type arg,
                    parallel_do_feeder<argument_type>& feeder) const {
        if (0 == (arg % 10000)) {
            mutex::scoped_lock coutLock(coutMutex);
            cout << "begin parallel_do arg = " << arg << endl;
        }

        buffer cycling;
        cycling.size = 0;
        cycling.cells = NULL;

        cycling = this->_source;

        for (size_t j = 0; j <= arg; j += 37) {
            for (size_t k = 0; k < cycling.size; ++k) {
                cycling.cells[k].bufferdword += this->_dwordInc.cells[k].bufferdword;
            }

            size_t cycleOffset = arg - j;
            size_t byteIncrements = (cycleOffset >= 36 ? 36 : cycleOffset);

            for (size_t k = 0; k < cycling.size; ++k) {
                cycling.cells[k].bufferbytes[0] +=
                    (this->_byteInc.cells[k].bufferbytes[0] * byteIncrements);
                cycling.cells[k].bufferbytes[1] +=
                    (this->_byteInc.cells[k].bufferbytes[1] * byteIncrements);
                cycling.cells[k].bufferbytes[2] +=
                    (this->_byteInc.cells[k].bufferbytes[2] * byteIncrements);
                cycling.cells[k].bufferbytes[3] +=
                    (this->_byteInc.cells[k].bufferbytes[3] * byteIncrements);
            }
        }

        if (this->_source == cycling || cycling.isZero()) {
            /*
            {
                mutex::scoped_lock coutLock(coutMutex);
                //cout << "found solution at " << dec << arg + 1 << endl;
            }
            */

            for (size_t old = _solution;
                 arg < old && _solution.compare_and_swap(arg + 1, old) != old;
                 old = _solution);

            /*
            {
                mutex::scoped_lock coutLock(coutMutex);
                cout << "solution = " << dec << _solution << endl;
            }
            */
        }
        /*
        else {
            if ((numeric_limits<size_t>::max() - arg) > this->_cycleIncrement) {
                size_t next = arg + this->_cycleIncrement;
                {
                    mutex::scoped_lock coutLock(coutMutex);
                    cout << "    arg = " << dec << arg << ", next = " << dec << next << endl;
                }

                if (next < _solution) {
                    feeder.add(next);
                }
            }
        }
        */

        /*
        {
            mutex::scoped_lock coutLock(coutMutex);
            //cout << "end parallel_do arg = " << arg << endl;

            cout << "Cycle " << dec << setw(4) << setfill('0') << arg + 1 << ": ";

            for (size_t j = 0; j < cycling.size; ++j) {
                if (j > 0) cout << " ";
                cout << hex << setw(8) << setfill('0') << uppercase << cycling.cells[j].bufferdword;
            }

            cout << endl;
        }
        */
    }
private:
    buffer _byteInc;
    size_t _cycleIncrement;
    buffer _dwordInc;
    buffer _source;
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

    /*
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
    */

    //parallel_do(range.begin(), range.end(), Apply(byteInc, 10, dwordInc, source));
    //cout << "InputIterator check: " << (InputIterator::end() == InputIterator::end()) << (InputIterator::end() != InputIterator::end()) << endl;
    parallel_do(InputIterator(0), InputIterator::end(), Apply(byteInc, 10, dwordInc, source));
    cout << dec << _solution << endl;
    cout << (tick_count::now() - begin).seconds() << endl;
    return 0;
}
