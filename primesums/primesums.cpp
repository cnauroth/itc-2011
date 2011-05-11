// Chris Nauroth
// Intel Threading Challenge 2011
// Prime Sums

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <tbb/tick_count.h>

using namespace std;

int main(int argc, char** argv) {
    tbb::tick_count begin = tbb::tick_count::now();

    if (argc < 5) {
        cerr << "Must specify range start, range end, max power, and output "
                "file name." << endl;
        return 1;
    }

    unsigned int rangeStart, rangeEnd, maxPower;
    istringstream(argv[1]) >> rangeStart;
    istringstream(argv[2]) >> rangeEnd;
    istringstream(argv[3]) >> maxPower;    
    const char* const outFileName = argv[4];

    unsigned int size = rangeEnd - rangeStart + 2;
    vector<bool> candidates(size, true);
    candidates[0] = false;
    candidates[1] = false; // 1 not considered prime

    for (size_t i = 2; i <= rangeEnd; ++i) {
        if (candidates[i]) {
            for (size_t j = i + i; j < rangeEnd; j += i) {
                candidates[j] = false;
            }
        }
    }

    for (size_t i = 1; i < candidates.size(); ++i)
        if (candidates[i])
            cout << i << endl;

    cout << (tbb::tick_count::now() - begin).seconds() << endl;
    return 0;
}
