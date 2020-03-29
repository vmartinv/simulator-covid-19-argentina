#include <iostream>
#include <random>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include "endian.hpp"
#include "progress_bar.hpp"
#include "population.hpp"
using namespace std;
namespace fs = boost::filesystem;

#define LOG(severity) BOOST_LOG_TRIVIAL(severity)
//#define DEBUG

const fs::path DATA_DIR = fs::path("data") / fs::path("argentina");
#ifdef DEBUG
const fs::path FAKE_DB_FILE = fs::path(DATA_DIR) / fs::path("fake_population_small.dat");
#else
const fs::path FAKE_DB_FILE = fs::path(DATA_DIR) / fs::path("fake_population.dat");
#endif

auto uniform = mt19937{random_device{}()};

enum PersonState{
    SUSCEPTIBLE = 0,
    EXPOSED,
    INFECTED_1,
    INFECTED_2,
    INFECTED_3,
    DEAD,
    RECOVERED,
    PERSON_STATE_COUNT
};
const int INFECTED_STATES_COUNT = 3;
const int MAX_AGE = 110;
static bool is_infected(const PersonState& s){
    return s == INFECTED_1 || s == INFECTED_2 || s == INFECTED_3;
}
enum Environments{
    HOME = 0,
    SCHOOL,
    WORK,
    NEIGHBOURHOOD,
    ENVIRONMENT_COUNT
};




typedef int PersonId;
typedef int EnvironmentId;

struct PersonSeirState{
    PersonState state;
    int index_on_general;
    EnvironmentId environment_id[ENVIRONMENT_COUNT];
    int index_on_environment[ENVIRONMENT_COUNT];
};
struct EnvironmentState{
    int num_inf[INFECTED_STATES_COUNT];
    vector<PersonId> susceptibles;
};


struct SeirState{
    Population population;
    vector<PersonId> general[PERSON_STATE_COUNT][MAX_AGE+1];
    vector<EnvironmentState> environments[ENVIRONMENT_COUNT];
    vector<PersonSeirState> estado_persona;

    SeirState(Population population): population(population) {
    }

    void reset(){
        LOG(info) << "Reseting state...";
        ProgressBar progressBar(population.people.size(), 70);
        for(int i=0; i<PERSON_STATE_COUNT; i++){
            for(int j=0; j<=MAX_AGE; j++){
                general[i][j].clear();
            }
        }
        estado_persona.resize(population.people.size());
        for(const auto& p: population.people){
            memset(&estado_persona[p.id], -1, sizeof(PersonSeirState));
            estado_persona[p.id].state = SUSCEPTIBLE;
            estado_persona[p.id].index_on_general = general[SUSCEPTIBLE][p.edad].size();
            general[SUSCEPTIBLE][p.edad].push_back(p.id);
            ++progressBar;
        }
        progressBar.done();
        generate_environments();
    }
    void generate_environments(){
        LOG(info) << "Generating environments...";
        ProgressBar progressBar(population.people.size(), 70);
        for(int i=0; i<ENVIRONMENT_COUNT; i++){
            environments[i].clear();
        }
        environments[HOME].resize(population.num_families);
        environments[NEIGHBOURHOOD].resize(population.num_zones);
        for(const auto& p: population.people){
            estado_persona[p.id].environment_id[HOME] = p.family;
            estado_persona[p.id].index_on_environment[HOME] = environments[HOME][p.family].susceptibles.size();
            environments[HOME][p.family].susceptibles.push_back(p.id);

            estado_persona[p.id].environment_id[NEIGHBOURHOOD] = p.zone;
            estado_persona[p.id].index_on_environment[NEIGHBOURHOOD] = environments[NEIGHBOURHOOD][p.zone].susceptibles.size();
            environments[NEIGHBOURHOOD][p.zone].susceptibles.push_back(p.id);
            
            ++progressBar;
        }
        progressBar.done();
    }

    void remove_from_general_list(const PersonId id){
        auto edad = population.get(id).edad;
        auto st = estado_persona[id].state;
        vector<PersonId>& vec = general[st][edad];
        int pos = estado_persona[id].index_on_general;
        if(pos != vec.size()-1){
            vec[pos] = vec.back();
            estado_persona[vec[pos]].index_on_general = pos;
        }
        vec.pop_back();
    }

    void add_to_general_list(const PersonId id){
        auto edad = population.get(id).edad;
        auto st = estado_persona[id].state;
        estado_persona[id].index_on_general = general[st][edad].size();
        general[st][edad].push_back(id);
    }

    void remove_from_susceptible_envs_lists(const PersonId id){
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            PersonSeirState& est_p = estado_persona[id];
            if(est_p.environment_id[env]!=-1){
                vector<PersonId>& vec = environments[env][est_p.environment_id[env]].susceptibles;
                int pos = est_p.index_on_environment[env];
                assert(pos!=-1);
                if(pos != vec.size()-1){
                    vec[pos] = vec.back();
                    estado_persona[vec[pos]].index_on_environment[env] = pos;
                }
                vec.pop_back();
            }
        }
    }

    void delta_infected_envs_count(const PersonId id, const int delta){
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            PersonSeirState& est_p = estado_persona[id];
            if(est_p.environment_id[env]!=-1){
                EnvironmentState &env_st = environments[env][estado_persona[id].environment_id[env]];
                env_st.num_inf[est_p.state]+= delta;
            }
        }
    }
    
    void change_state(const PersonId &id, const PersonState &nw){
        const PersonState old = estado_persona[id].state;
        if(old==nw){
            return;
        }
        remove_from_general_list(id);
        if(old==SUSCEPTIBLE){
            remove_from_susceptible_envs_lists(id);
        }
        if(is_infected(old)){
            delta_infected_envs_count(id, -1);
        }
        estado_persona[id].state = nw;
        if(is_infected(nw)){
            delta_infected_envs_count(id, 1);
        }
        add_to_general_list(id);
    }
};

void do_simulation(SeirState &state, int days=256){
    state.reset();
    LOG(info) << "Starting simulation...";
    for(int day = 1; day<=days; day++){
        for(int age=0; age<=MAX_AGE; age++){
            vector<PersonId> &i3 = state.general[INFECTED_3][age];
            vector<PersonId> deaths;
            int expected_deaths = round(i3.size()*0.01);
            sample(begin(i3), end(i3), back_inserter(deaths), expected_deaths, uniform);
            for(const PersonId id: deaths){
                state.change_state(id, DEAD);
            }
        }
    }
}

int main(){
    srand(time(NULL));
    SeirState state(Population(FAKE_DB_FILE.string()));
    state.population.report();
    do_simulation(state);
    return 0;
}
