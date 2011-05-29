// Chris Nauroth
// Intel Threading Challenge 2011
// Prime Sums

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <tbb/pipeline.h>
#include <tbb/tick_count.h>

using namespace std;
using namespace tbb;

class PrimeFunctor {
public:
    PrimeFunctor(vector<bool>& candidates, const size_t rangeStart,
                 const size_t rangeEnd):
        _candidates(candidates), _rangeStart(rangeStart), _rangeEnd(rangeEnd),
        _i(2) {
    }

    size_t operator()(flow_control& fc) const {
        size_t nextPrime = 0;
        size_t i = this->_i;

        for (; 0 == nextPrime && i <= this->_rangeEnd; ++i) {
            if (this->_candidates[i]) {
                for (size_t j = i * i; j <= this->_rangeEnd; j += i) {
                    this->_candidates[j] = false;
                }

                nextPrime = i;
            }
        }

        this->_i = i;

        if (0 == nextPrime)
            fc.stop();

        return nextPrime;
    }

private:
    vector<bool>& _candidates;
    const size_t _rangeStart, _rangeEnd;
    mutable size_t _i;
};

struct PerfectPower {
    size_t start, end, sum, base, power;
};

class PerfectPowerFunctor {
public:
    PerfectPowerFunctor(vector<bool>& candidates, const size_t maxPower,
                        const size_t rangeStart):
        _candidates(candidates), _maxPower(maxPower), _rangeStart(rangeStart) {
    }

    vector<PerfectPower> operator()(const size_t prime) const {
        vector<PerfectPower> perfectPowers;

        for (size_t sum = prime, j = prime - 1; j >= this->_rangeStart; --j) {
            if (this->_candidates[j]) {
                sum += j;

                for (size_t base = 2, product = base * base;
                     product <= sum;
                     ++base, product = base * base) {

                    for (size_t power = 2;
                         power <= this->_maxPower && product <= sum;
                         ++power, product *= base) {

                        if (product == sum) {
                            PerfectPower perfectPower;
                            perfectPower.start = j;
                            perfectPower.end = prime;
                            perfectPower.sum = sum;
                            perfectPower.base = base;
                            perfectPower.power = power;
                            perfectPowers.push_back(perfectPower);
                        }
                    }
                }
            }
        }

        return perfectPowers;
    }
private:
    vector<bool>& _candidates;
    const size_t _maxPower;
    const size_t _rangeStart;
};

class OutputFunctor {
public:
    OutputFunctor(ostream& out):_out(out) {
    }

    void operator()(const vector<PerfectPower> perfectPowers) const {
        for (vector<PerfectPower>::const_iterator i = perfectPowers.begin();
             i != perfectPowers.end(); ++i) {

            this->_out << "sum(" << i->start << ":" << i->end
                       << ") = " << i->sum << " = " << i->base
                       << "**" << i->power << endl;
        }
    }
private:
    ostream& _out;
};

int main(int argc, char** argv) {
    tick_count begin = tick_count::now();

    if (argc < 5) {
        cerr << "Must specify range start, range end, max power, and output "
                "file name." << endl;
        return 1;
    }

    unsigned int rangeStart, rangeEnd, maxPower;
    istringstream(argv[1]) >> rangeStart;
    istringstream(argv[2]) >> rangeEnd;
    istringstream(argv[3]) >> maxPower;    
    ofstream out(argv[4]);

    vector<bool> candidates(rangeEnd + 1, true);
    candidates[0] = false;
    candidates[1] = false; // 1 not considered prime

    filter_t<void, size_t> f1(filter::serial_in_order,
                              PrimeFunctor(candidates, rangeStart, rangeEnd));

    filter_t<size_t, vector<PerfectPower> > f2(filter::parallel,
                                               PerfectPowerFunctor(candidates, maxPower, rangeStart));

    filter_t<vector<PerfectPower>, void> f3(filter::serial_in_order,
                                            OutputFunctor(out));

    int ntoken = 10;
    parallel_pipeline(ntoken, f1 & f2 & f3);
    cout << (tick_count::now() - begin).seconds() << endl;
    return 0;
}
