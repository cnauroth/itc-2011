// Chris Nauroth
// Intel Threading Challenge 2011
// Prime Sums

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <tbb/concurrent_vector.h>
#include <tbb/pipeline.h>
#include <tbb/tick_count.h>

using namespace std;
using namespace tbb;

static concurrent_vector<size_t> primes;

class PrimeFunctor {
public:
    PrimeFunctor(const size_t rangeStart, const size_t rangeEnd):
        _candidates(vector<bool>(rangeEnd + 1, true)), _rangeStart(rangeStart), _rangeEnd(rangeEnd) {

        this->_candidates[0] = false;
        this->_candidates[1] = false;
        this->_candidates[2] = true;
        primes.push_back(2);

        for (size_t i = 2 * 2; i <= this->_rangeEnd; i += 2) {
            this->_candidates[i] = false;
        }

        this->_i = 3;
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

        size_t nextIndex = 0;
        if (0 == nextPrime) {
            fc.stop();
        }
        else {
            primes.push_back(nextPrime);
            nextIndex = primes.size() - 1;
        }

        return nextIndex;
    }

private:
    mutable vector<bool> _candidates;
    const size_t _rangeStart, _rangeEnd;
    mutable size_t _i;
};

struct PerfectPower {
    size_t start, end, sum, base, power;
};

class PerfectPowerFunctor {
public:
    PerfectPowerFunctor(const size_t maxPower, const size_t rangeStart):
        _maxPower(maxPower), _rangeStart(rangeStart) {
    }

    vector<PerfectPower> operator()(const size_t index) const {
        vector<PerfectPower> perfectPowers;

        size_t sum = primes[index];
        for (size_t j = index - 1; primes[j] >= this->_rangeStart; --j) {
            sum += primes[j];

            for (size_t base = 2, product = base * base;
                 product <= sum;
                 ++base, product = base * base) {

                for (size_t power = 2;
                     power <= this->_maxPower && product <= sum;
                     ++power, product *= base) {

                    if (product == sum) {
                        PerfectPower perfectPower;
                        perfectPower.start = primes[j];
                        perfectPower.end = primes[index];
                        perfectPower.sum = sum;
                        perfectPower.base = base;
                        perfectPower.power = power;
                        perfectPowers.push_back(perfectPower);
                    }
                }
            }

            if (0 == j) break;
        }

        return perfectPowers;
    }
private:
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

    unsigned int rangeStart, rangeEnd, maxPower, ntoken;
    istringstream(argv[1]) >> rangeStart;
    istringstream(argv[2]) >> rangeEnd;
    istringstream(argv[3]) >> maxPower;    
    ofstream out(argv[4]);

    if (argc > 5)
        istringstream(argv[5]) >> ntoken;
    else
        ntoken = 10;

    filter_t<void, size_t> f1(filter::serial_in_order,
                              PrimeFunctor(rangeStart, rangeEnd));

    filter_t<size_t, vector<PerfectPower> > f2(filter::parallel,
                                               PerfectPowerFunctor(maxPower, rangeStart));

    filter_t<vector<PerfectPower>, void> f3(filter::serial_in_order,
                                            OutputFunctor(out));
    parallel_pipeline(ntoken, f1 & f2 & f3);

    cout << (tick_count::now() - begin).seconds() << endl;
    return 0;
}
