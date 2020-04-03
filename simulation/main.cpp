#include <iostream>
#include <boost/filesystem.hpp>
#include "population.hpp"
#include "seir_simulation.hpp"
using namespace std;
namespace fs = boost::filesystem;

const fs::path DATA_DIR = fs::path("data") / fs::path("argentina");
#ifdef DEBUG
const fs::path FAKE_DB_FILE = fs::path(DATA_DIR) / fs::path("fake_population_small.dat");
#else
const fs::path FAKE_DB_FILE = fs::path(DATA_DIR) / fs::path("fake_population.dat");
#endif

int main(){
    SeirSimulation simulation(Population(FAKE_DB_FILE.string()));
    simulation.run();
    return 0;
}
