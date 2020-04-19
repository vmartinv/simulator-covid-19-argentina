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
    COUNTRY,
    BY_AGE,
    ENVIRONMENT_COUNT
};
const string environment_text[]={
    "HOME",
    "SCHOOL",
    "WORK",
    "NEIGHBOURHOOD",
    "COUNTRY",
    "BY_AGE"
};
typedef int EnvironmentId;

struct PersonSeirState{
    PersonState state;
    EnvironmentId environment_id[ENVIRONMENT_COUNT];
    unsigned index_on_environment[ENVIRONMENT_COUNT];
};
struct EnvironmentState{
    vector<PersonId> people[PERSON_STATE_COUNT];
};

class SeirState{
public:
    const Population population;

    SeirState(Population population): population(population) {
        assert(PERSON_STATE_COUNT == sizeof(person_state_text)/sizeof(person_state_text[0]));
        assert(ENVIRONMENT_COUNT == sizeof(environment_text)/sizeof(environment_text[0]));
    }

    void reset(){
        LOG(info) << "Reseting state...";
        ProgressBar progressBar(population.people.size());
        person_states.resize(population.people.size());
        for(const auto& p: population.people){
            memset(&person_states[p.id], -1, sizeof(PersonSeirState));
            person_states[p.id].state = SUSCEPTIBLE;
            ++progressBar;
        }
        progressBar.done();
        generate_environments();
    }

    bool change_state(const PersonId &id, const PersonState &from, const PersonState &nw){
        const PersonState old = person_states[id].state;
        if(old==nw || old!=from){
            return false;
        }
        remove_from_environments(id);
        person_states[id].state = nw;
        add_to_environments(id);
        return true;
    }

    unsigned count_state(const PersonState& st){
        return environments[COUNTRY][0].people[st].size();
    }

    const vector<PersonSeirState> &get_person_states() const{
        return person_states;
    }

    const vector<EnvironmentState> &get_environments(const Environments &env) const {
        return environments[env];
    }
private:
    vector<PersonSeirState> person_states;
    vector<EnvironmentState> environments[ENVIRONMENT_COUNT];

    void generate_environments(){
        LOG(info) << "Generating environments...";
        ProgressBar progressBar(population.people.size());
        for(int i=0; i<ENVIRONMENT_COUNT; i++){
            environments[i].clear();
        }
        environments[HOME].resize(population.num_families);
        environments[NEIGHBOURHOOD].resize(population.num_zones);
        environments[SCHOOL].resize(population.num_schools);
        environments[COUNTRY].resize(1);
        environments[BY_AGE].resize(population.max_age+1);
        for(const auto& p: population.people){
            person_states[p.id].environment_id[HOME] = p.family;

            const unsigned zone = population.families[p.family].zone;
            person_states[p.id].environment_id[NEIGHBOURHOOD] = zone;

            if(p.escuela!=Population::NO_SCHOOL){
                person_states[p.id].environment_id[SCHOOL] = p.escuela;
            }

            person_states[p.id].environment_id[COUNTRY] = 0;

            person_states[p.id].environment_id[BY_AGE] = int(p.edad);
            add_to_environments(p.id);
            ++progressBar;
        }
        progressBar.done();
    }

    void remove_from_environments(const PersonId id){
        PersonSeirState& est_p = person_states[id];
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            if(est_p.environment_id[env]!=-1){
                vector<PersonId>& vec = environments[env][est_p.environment_id[env]].people[est_p.state];
                unsigned pos = est_p.index_on_environment[env];
                assert(pos<vec.size());
                assert(vec[pos]==id);
                if(pos != vec.size()-1 && vec.size()>1){
                    vec[pos] = vec.back();
                    person_states[vec[pos]].index_on_environment[env] = pos;
                }
                vec.pop_back();
                est_p.index_on_environment[env] = -1;
            }
        }
    }

    void add_to_environments(const PersonId id){
        PersonSeirState& est_p = person_states[id];
        for(int env=0; env<ENVIRONMENT_COUNT; env++){
            if(est_p.environment_id[env]!=-1){
                vector<PersonId>& vec = environments[env][est_p.environment_id[env]].people[est_p.state];
                est_p.index_on_environment[env] = vec.size();
                vec.push_back(id);
            }
        }
    }
};
#endif
