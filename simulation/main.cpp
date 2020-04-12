#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include "common.hpp"
#include "progress_bar.hpp"
#include "population.hpp"
#include "disease_parameters.hpp"
#include "seir_simulation.hpp"
using namespace std;
namespace fs = boost::filesystem;
using namespace boost::program_options;
namespace logging = boost::log;

const fs::path DATA_DIR = fs::path("..") / fs::path("data") / fs::path("argentina");
#ifdef DEBUG
const fs::path FAKE_DB_FILE = DATA_DIR / fs::path("fake_population_small");
#else
const fs::path FAKE_DB_FILE = DATA_DIR / fs::path("fake_population");
#endif
const fs::path JSON_OUTPUT_FILE = DATA_DIR / fs::path("simulation_results.json");

int main(int argc, const char *argv[]){
    variables_map vm;
    try {
        options_description desc{"Options"};
        desc.add_options()
        ("help,h", "Help screen")
        ("silent", "Silent mode")
        ("seed,s", value<int>()->default_value(0), "Seed used by the simulation (0 means random seed)")
        ("days,d", value<unsigned>()->default_value(30), "Numbers of days to simulate")
        ("population,p", value<string>()->default_value(FAKE_DB_FILE.string()), "Basename of the fake population database (created by fake_population_generator.py)")
        ("json,j", value<string>()->default_value(JSON_OUTPUT_FILE.string()), "Where to save the simulation reports");

        store(parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")){
            cout << "Runs SEIR simulation over a fake population dataset.\n";
            cout << desc << '\n';
            return 0;
        }
    }
    catch (const error &ex)
    {
        cerr << ex.what() << '\n';
    }
    if(vm.count("silent")){
        logging::core::get()->set_filter
        (
            logging::trivial::severity >= logging::trivial::error
        );
        ProgressBar::show = false;
    }
#ifdef DEBUG
    LOG(info) << "Compiled in Debug mode";
#else
    LOG(info) << "Compiled in Release mode";
#endif
    const string population_filename = vm["population"].as<string>();
    SeirSimulation simulation(
        Population(population_filename+".dat", population_filename+".json"),
        DiseaseParameters(),
        vm["seed"].as<int>()
    );
    simulation.run(
        vm["days"].as<unsigned>(),
        vm["json"].as<string>()
    );
    return 0;
}
