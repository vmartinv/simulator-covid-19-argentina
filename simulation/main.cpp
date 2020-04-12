#include <boost/filesystem.hpp>
#include "common.hpp"
#include "population.hpp"
#include "disease_parameters.hpp"
#include "seir_simulation.hpp"
using namespace std;
namespace fs = boost::filesystem;

const fs::path DATA_DIR = fs::path("..") / fs::path("data") / fs::path("argentina");
#ifdef DEBUG
const fs::path FAKE_DB_FILE = DATA_DIR / fs::path("fake_population_small.dat");
const fs::path FAKE_DB_JSON_FILE = DATA_DIR / fs::path("fake_population_small.json");
#else
const fs::path FAKE_DB_FILE = DATA_DIR / fs::path("fake_population.dat");
const fs::path FAKE_DB_JSON_FILE = DATA_DIR / fs::path("fake_population.json");
#endif

int main(){
#ifdef DEBUG
    LOG(info) << "Running in Debug mode";
#else
    LOG(info) << "Running in Release mode";
#endif
    SeirSimulation simulation(Population(FAKE_DB_FILE.string(), FAKE_DB_JSON_FILE.string()), DiseaseParameters(), 23415);
    simulation.run(30);
    return 0;
}
