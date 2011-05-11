// Chris Nauroth
// Intel Threading Challenge 2011
// Maze of Life

// This solution works by attempting every possible move recursively until it
// finds a path to the goal or exhausts all possibilities.  At each step, the
// next set of possible moves are enqueued to a priority queue, responsible for
// ordering the moves according to a simple heuristic: favor moves that bring
// the intelligent cell closer to the goal in fewer moves.  Multiple threads
// process moves from the priority queue concurrently, using parallel_for from
// Threading Building Blocks.  The STL priority_queue is not thread-safe, so
// access is controlled by locking a mutex.  This solution eagerly seeks a
// solution path in minimal time, but it does not always find the shortest path.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>

#include <tbb/concurrent_vector.h>
#include <tbb/mutex.h>
#include <tbb/parallel_while.h>
#include <tbb/tick_count.h>

using namespace std;

enum Cell {
    DEAD,
    ALIVE
};

typedef vector<vector<Cell> > CellMatrix;

struct Point {
    int x, y;
};

struct Grid {
    CellMatrix cells;
    size_t goalX, goalY, iX, iY;
    size_t dimX, dimY;

    bool operator==(const Grid& other) const {
        if (this->goalX == other.goalX &&
            this->goalY == other.goalY &&
            this->iX == other.iX &&
            this->iY == other.iY &&
            this->dimX == other.dimX &&
            this->dimY == other.dimY) {

            for (size_t y = 0; y < this->cells.size(); ++y) {
                for (size_t x = 0; x < this->cells[y].size(); ++x) {
                    if (this->cells[y][x] != other.cells[y][x])
                        return false;
                }
            }
        }
        else {
            return false;
        }

        return true;
    }
};

struct Game {
    Grid grid;
    vector<int> moves;

    // implements a simple heuristic for scoring a move based on current
    // distance from the goal and the number of moves so far
    size_t score() const {
        size_t x = grid.goalX - grid.iX;
        size_t y = grid.goalY - grid.iY;
        return (x * x) + (y * y) + this->moves.size();
    }

    // used to prioritize Game instances to check within a priority_queue
    bool operator<(const Game& other) const {
        return this->score() >= other.score();
    }
};

class GameCompare {
public:
    bool operator()(Game*& lhs, Game*& rhs) const {
        return *lhs < *rhs;
    }
};

bool isLoss(const Game& game);
bool isWin(const Game& game);
class Apply;
void queueNextMoves(const Game& game, tbb::parallel_while<Apply>& parallelWhile);

// priority queue for ordering games to check and associated functions
static tbb::mutex gameQueueMutex;
static tbb::concurrent_vector<Grid> visitedGrids;
static priority_queue<Game*, vector<Game*>, GameCompare> gameQueue;
static Game* dequeueGame();
static void enqueueGame(Game* game, tbb::parallel_while<Apply>& parallelWhile);


static volatile bool solutionFound = false;
static tbb::mutex solutionMutex;
static vector<int> solution;
static vector<int> getSolution();
static void setSolution(const vector<int>& solution);

// somewhat bogus stream to bootstrap a parallel_while by returning true once
// and only once, relying on loop body to queue up subsequent work with
// parallel_while.add
class Stream {
public:
    Stream():_present(true) {
    }

    bool pop_if_present(bool& next) {
        next = this->_present;
        this->_present = false;
        return next;
    }
private:
    volatile bool _present;
};

// parallel_while loop body
class Apply {
public:
    typedef bool argument_type;

    Apply(tbb::parallel_while<Apply>& parallelWhile):_parallelWhile(parallelWhile) {
    }

    void operator()(argument_type arg) const {
        if (arg) {
            Game* game = dequeueGame();

            if (!solutionFound) {
                if (NULL != game) {
                    if (isWin(*game)) {
                        setSolution(game->moves);
                        solutionFound = true;
                    }
                    else if (!isLoss(*game)) {
                        // check if we have already visited this grid before to
                        // prevent infinite cycles
                        bool found = false;

                        for (tbb::concurrent_vector<Grid>::const_iterator i = visitedGrids.begin();
                             i != visitedGrids.end(); ++i) {

                            if (*i == game->grid) {
                                found = true;
                                break;
                            }
                        }

                        if (!found) {
                            visitedGrids.push_back(game->grid);
                            queueNextMoves(*game, _parallelWhile);
                        }
                    }
                }
            }

            delete game;
        }
    }
private:
    tbb::parallel_while<Apply>& _parallelWhile;
};

// thread-safe dequeue of Game from priority_queue
static Game* dequeueGame() {
    tbb::mutex::scoped_lock lock(gameQueueMutex);

    if (gameQueue.empty()) {
        return NULL;
    }
    else {
        Game* game = gameQueue.top();
        gameQueue.pop();
        return game;
    }
}

// thread-safe enqueue of Game to priority_queue
static void enqueueGame(Game* game, tbb::parallel_while<Apply>& parallelWhile) {
    {
        tbb::mutex::scoped_lock lock(gameQueueMutex);
        gameQueue.push(game);
    }

    parallelWhile.add(true);
}

// thread-safe get of solution
static vector<int> getSolution() {
    tbb::mutex::scoped_lock lock(solutionMutex);
    return solution;
}

// thread-safe set of solution
static void setSolution(const vector<int>& newSolution) {
    tbb::mutex::scoped_lock lock(solutionMutex);

    if (!solutionFound || (newSolution.size() < solution.size()))
        solution = newSolution;
}

void printGrid(const Grid& grid) {
    cout << "goalX = " << grid.goalX << endl;
    cout << "goalY = " << grid.goalY << endl;
    cout << "iX = " << grid.iX << endl;
    cout << "iY = " << grid.iY << endl;

    for (CellMatrix::const_iterator i = grid.cells.begin();
         i != grid.cells.end(); ++i) {

        for (vector<Cell>::const_iterator j = i->begin();
             j != i->end(); ++j) {

            cout << *j << ' ';
        }

        cout << endl;
    }
}

bool isLoss(const Game& game) {
    return (DEAD == game.grid.cells[game.grid.iY][game.grid.iX]);
}

bool isWin(const Game& game) {
    return (ALIVE == game.grid.cells[game.grid.iY][game.grid.iX]) &&
        (game.grid.iX == game.grid.goalX && game.grid.iY == game.grid.goalY);
}

void move(const Game& current, Game& next, const Point& nextMove) {
    next.grid.dimX = current.grid.dimX;
    next.grid.dimY = current.grid.dimY;
    next.grid.goalX = current.grid.goalX;
    next.grid.goalY = current.grid.goalY;
    next.grid.iX = nextMove.x;
    next.grid.iY = nextMove.y;
    next.grid.cells.resize(current.grid.dimY);
    for (CellMatrix::iterator i = next.grid.cells.begin();
         i != next.grid.cells.end(); ++i) {

        i->resize(current.grid.dimX);
    }

    for (size_t y = 0; y < next.grid.cells.size(); ++y) {
        for (size_t x = 0; x < next.grid.cells[y].size(); ++x) {
            int aliveNeighborCount = 0;

            if (y > 0) {
                if (x > 0)
                    if ((x - 1) == next.grid.iX && (y - 1) == next.grid.iY)
                        ++aliveNeighborCount;
                    else if (!((x - 1) == current.grid.iX && (y - 1) == current.grid.iY))
                        if (ALIVE == current.grid.cells[y - 1][x - 1])
                            ++aliveNeighborCount;

                if (x == next.grid.iX && (y - 1) == next.grid.iY)
                    ++aliveNeighborCount;
                else if (!(x == current.grid.iX && (y - 1) == current.grid.iY))
                    if (ALIVE == current.grid.cells[y - 1][x])
                        ++aliveNeighborCount;

                if (x < (current.grid.dimX - 1))
                    if ((x + 1) == next.grid.iX && (y - 1) == next.grid.iY)
                        ++aliveNeighborCount;
                    else if (!((x + 1) == current.grid.iX && (y - 1) == current.grid.iY))
                        if (ALIVE == current.grid.cells[y - 1][x + 1])
                            ++aliveNeighborCount;
            }

            if (x > 0)
                if ((x - 1) == next.grid.iX && y == next.grid.iY)
                    ++aliveNeighborCount;
                else if (!((x - 1) == current.grid.iX && y == current.grid.iY))
                    if (ALIVE == current.grid.cells[y][x - 1])
                        ++aliveNeighborCount;

            if (x < (current.grid.dimX - 1)) {
                if ((x + 1) == next.grid.iX && y == next.grid.iY)
                    ++aliveNeighborCount;
                else if (!((x + 1) == current.grid.iX && y == current.grid.iY))
                    if (ALIVE == current.grid.cells[y][x + 1])
                        ++aliveNeighborCount;
            }

            if (y < (current.grid.dimY - 1)) {
                if (x > 0)
                    if ((x - 1) == next.grid.iX && (y + 1) == next.grid.iY)
                        ++aliveNeighborCount;
                    else if (!((x - 1) == current.grid.iX && (y + 1) == current.grid.iY))
                        if (ALIVE == current.grid.cells[y + 1][x - 1])
                            ++aliveNeighborCount;

                if (x == next.grid.iX && (y + 1) == next.grid.iY)
                    ++aliveNeighborCount;
                else if (!(x == current.grid.iX && (y + 1) == current.grid.iY))
                    if (ALIVE == current.grid.cells[y + 1][x])
                        ++aliveNeighborCount;

                if (x < (current.grid.dimX - 1))
                    if ((x + 1) == next.grid.iX && (y + 1) == next.grid.iY)
                        ++aliveNeighborCount;
                    else if (!((x + 1) == current.grid.iX && (y + 1) == current.grid.iY))
                        if (ALIVE == current.grid.cells[y + 1][x + 1])
                            ++aliveNeighborCount;
            }

            if (aliveNeighborCount < 2)
                next.grid.cells[y][x] = DEAD;
            else if (aliveNeighborCount == 2)
                if (x == next.grid.iX && y == next.grid.iY)
                    next.grid.cells[y][x] = ALIVE;
                else if (x == current.grid.iX && y == current.grid.iY)
                    next.grid.cells[y][x] = DEAD;
                else
                    next.grid.cells[y][x] = current.grid.cells[y][x];
            else if (aliveNeighborCount == 3)
                next.grid.cells[y][x] = ALIVE;
            else if (aliveNeighborCount > 3)
                next.grid.cells[y][x] = DEAD;
        }
    }
}

void queueNextMoves(const Game& game, tbb::parallel_while<Apply>& parallelWhile) {
    Point nextMove;
    Game* next = new Game;

    nextMove.x = game.grid.iX;
    nextMove.y = game.grid.iY;
    move(game, *next, nextMove);
    next->moves = game.moves;
    next->moves.push_back(0);
    enqueueGame(next, parallelWhile);

    if (game.grid.iX > 0) {
        nextMove.x = game.grid.iX - 1;

        if (game.grid.iY > 0) {
            nextMove.y = game.grid.iY - 1;

            if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
                next = new Game;
                move(game, *next, nextMove);
                next->moves = game.moves;
                next->moves.push_back(1);
                enqueueGame(next, parallelWhile);
            }
        }

        nextMove.y = game.grid.iY;

        if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
            next = new Game;
            move(game, *next, nextMove);
            next->moves = game.moves;
            next->moves.push_back(8);
            enqueueGame(next, parallelWhile);
        }

        if (game.grid.iY < game.grid.dimY - 1) {
            nextMove.y = game.grid.iY + 1;

            if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
                next = new Game;
                move(game, *next, nextMove);
                next->moves = game.moves;
                next->moves.push_back(7);
                enqueueGame(next, parallelWhile);
            }
        }
    }

    nextMove.x = game.grid.iX;

    if (game.grid.iY > 0) {
        nextMove.y = game.grid.iY - 1;

        if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
            next = new Game;
            move(game, *next, nextMove);
            next->moves = game.moves;
            next->moves.push_back(2);
            enqueueGame(next, parallelWhile);
        }
    }

    if (game.grid.iY < game.grid.dimY - 1) {
        nextMove.y = game.grid.iY + 1;

        if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
            next = new Game;
            move(game, *next, nextMove);
            next->moves = game.moves;
            next->moves.push_back(6);
            enqueueGame(next, parallelWhile);
        }
    }

    if (game.grid.iX < game.grid.dimX - 1) {
        nextMove.x = game.grid.iX + 1;

        if (game.grid.iY > 0) {
            nextMove.y = game.grid.iY - 1;

            if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
                next = new Game;
                move(game, *next, nextMove);
                next->moves = game.moves;
                next->moves.push_back(3);
                enqueueGame(next, parallelWhile);
            }
        }

        nextMove.y = game.grid.iY;

        if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
            next = new Game;
            move(game, *next, nextMove);
            next->moves = game.moves;
            next->moves.push_back(4);
            enqueueGame(next, parallelWhile);
        }

        if (game.grid.iY < game.grid.dimY - 1) {
            nextMove.y = game.grid.iY + 1;

            if (DEAD == game.grid.cells[nextMove.y][nextMove.x]) {
                next = new Game;
                move(game, *next, nextMove);
                next->moves = game.moves;
                next->moves.push_back(5);
                enqueueGame(next, parallelWhile);
            }
        }
    }
}

void findSolution(const Grid& grid) {
    Game* initGame = new Game;
    initGame->grid = grid;
    gameQueue.push(initGame);
    tbb::parallel_while<Apply> parallelWhile;
    Stream stream;
    Apply apply(parallelWhile);
    parallelWhile.run(stream, apply);
}

void tokenizeInts(const char* const line, vector<int>& ints) {
    istringstream iss(line);
    vector<string> tokens;

    copy(istream_iterator<string>(iss),
         istream_iterator<string>(),
         back_inserter<vector<string> >(tokens));

    for (vector<string>::const_iterator i = tokens.begin();
         i != tokens.end(); ++i) {

        stringstream ss(*i);
        int x;
        ss >> x;
        ints.push_back(x);
    }
}

void readGridFromInput(const char* const inFileName, Grid& grid) {
    char buffer[1024];
    vector<int> ints;
    ifstream in(inFileName);

    in.getline(buffer, 1024);
    tokenizeInts(buffer, ints);
    int dimY = ints[0];
    int dimX = ints[1];
    grid.dimX = dimX;
    grid.dimY = dimY;
    grid.cells.resize(dimY);
    for (CellMatrix::iterator i = grid.cells.begin();
         i != grid.cells.end(); ++i) {

        i->resize(dimX);
    }

    in.getline(buffer, 1024);
    ints.clear();
    tokenizeInts(buffer, ints);
    grid.goalY = ints[0] - 1;
    grid.goalX = ints[1] - 1;
    ints.clear();

    in.getline(buffer, 1024);
    ints.clear();
    tokenizeInts(buffer, ints);
    grid.iY = ints[0] - 1;
    grid.iX = ints[1] - 1;
    grid.cells[grid.iY][grid.iX] = ALIVE;

    bool done = false;
    while (!in.eof() && !done) {
        in.getline(buffer, 1024);
        ints.clear();
        tokenizeInts(buffer, ints);

        for (vector<int>::const_iterator i = ints.begin();
             i != ints.end(); ++i) {

            int cellY = *i;
            ++i;
            int cellX = *i;

            if (0 == cellX && 0 == cellY) {
                done = true;
            }
            else {
                --cellY;
                --cellX;
                grid.cells[cellY][cellX] = ALIVE;
            }
        }
    }

    in.close();
}

void writeSolutionToOutput(const char* const outFileName) {
    ofstream out(outFileName);

    if (solutionFound) {
        vector<int> moves = getSolution();

        for (vector<int>::const_iterator i = moves.begin();
             i != moves.end(); ++i) {

            out << *i;
        }
    }
    else {
        out << "no solution" << endl;
    }

    out.close();
}

int main(int argc, char** argv) {
    tbb::tick_count begin = tbb::tick_count::now();

    if (argc < 3) {
        cerr << "Must specify input file and output file." << endl;
        return 1;
    }

    const char* const inFileName = argv[1];
    const char* const outFileName = argv[2];

    Grid grid;
    readGridFromInput(inFileName, grid);
    findSolution(grid);
    writeSolutionToOutput(outFileName);
    cout << (tbb::tick_count::now() - begin).seconds() << endl;
    return 0;
}
