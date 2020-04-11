#ifndef SEIR_STATE_HPP
#define SEIR_STATE_HPP
#include <random>
#include <vector>
#include "common.hpp"
#include "endian.hpp"
#include "progress_bar.hpp"
#include "population.hpp"
using namespace std;

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
const string person_state_text[]={
    "SUSCEPTIBLE",
    "EXPOSED",
    "INFECTED_1",
    "INFECTED_2",
    "INFECTED_3",
    "DEAD",
    "RECOVERED"
};
inline static bool is_infected(const PersonState& s){
    return s == INFECTED_1 || s == INFECTED_2 || s == INFECTED_3;
}
enum Environments{
    HOME = 0,
    SCHOOL,
    WORK,
    NEIGHBOURHOOD,
    ENVIRONMENT_COUNT
};
const string environment_text[]={
    "HOME",
    "SCHOOL",
    "WORK",
    "NEIGHBOURHOOD",
};
typedef int EnvironmentId;

struct PersonSeirState{
    PersonState state;
    unsigned index_on_general;
    EnvironmentId environment_id[ENVIRONMENT_COUNT];
    unsigned index_on_environment[ENVIRONMENT_COUNT];
};
struct EnvironmentState{
    unsigned num[PERSON_STATE_COUNT];
    vector<PersonId> susceptibles;
};

class SeirState{
public:
    Population population;
    vector<PersonId> general[PERSON_STATE_COUNT][MAX_AGE+1];
    vector<EnvironmentState> environments[ENVIRONMENT_COUNT];

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

    unsigned count_state(const PersonState& st){
        unsigned count = 0;
        for(auto age=0; age<=MAX_AGE; age++){
            count += general[st][age].size();
        }
        return count;
    }
private:


    vector<PersonSeirState> estado_persona;
    void generate_environments(){
        LOG(info) << "Generating environments...";
        ProgressBar progressBar(population.people.size(), 70);
        for(int i=0; i<ENVIRONMENT_COUNT; i++){
            environments[i].clear();
        }
        environments[HOME].resize(population.num_families);
        environments[NEIGHBOURHOOD].resize(population.num_zones);
        environments[SCHOOL].resize(population.num_schools);
        for(const auto& p: population.people){
            estado_persona[p.id].environment_id[HOME] = p.family;
            estado_persona[p.id].index_on_environment[HOME] = environments[HOME][p.family].susceptibles.size();
            environments[HOME][p.family].susceptibles.push_back(p.id);

            estado_persona[p.id].environment_id[NEIGHBOURHOOD] = p.zone;
            estado_persona[p.id].index_on_environment[NEIGHBOURHOOD] = environments[NEIGHBOURHOOD][p.zone].susceptibles.size();
            environments[NEIGHBOURHOOD][p.zone].susceptibles.push_back(p.id);

            if(p.escuela!=Population::NO_SCHOOL){
                estado_persona[p.id].environment_id[SCHOOL] = p.escuela;
                estado_persona[p.id].index_on_environment[SCHOOL] = environments[SCHOOL][p.escuela].susceptibles.size();
                environments[SCHOOL][p.escuela].susceptibles.push_back(p.id);
            }
            ++progressBar;
        }
        progressBar.done();
    }

    void remove_from_general_list(const PersonId id){
        auto edad = population.get(id).edad;
        auto st = estado_persona[id].state;
        vector<PersonId>& vec = general[st][edad];
        unsigned pos = estado_persona[id].index_on_general;
        if(pos != vec.size()-1 && vec.size()>1){
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
        PersonSeirState& est_p = estado_persona[id];
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            if(est_p.environment_id[env]!=-1){
                vector<PersonId>& vec = environments[env][est_p.environment_id[env]].susceptibles;
                unsigned pos = est_p.index_on_environment[env];
                assert(pos<vec.size());
                assert(vec[pos]==id);
                if(pos != vec.size()-1 && vec.size()>1){
                    vec[pos] = vec.back();
                    estado_persona[vec[pos]].index_on_environment[env] = pos;
                }
                vec.pop_back();
                est_p.index_on_environment[env] = -1;
            }
        }
    }

    void delta_infected_envs_count(const PersonId id, const int delta){
        PersonSeirState& est_p = estado_persona[id];
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            if(est_p.environment_id[env]!=-1){
                EnvironmentState &env_st = environments[env][est_p.environment_id[env]];
                env_st.num[INFECTED_1] += delta;
            }
        }
    }
};
#endif
