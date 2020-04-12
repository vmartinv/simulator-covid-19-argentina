#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include "progress_bar.hpp"
#include "seir_state.hpp"
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;

class SeirSimulation{
    enum TransitionReason{
        HOME_CONTACT = 0,
        SCHOOL_CONTACT,
        WORK_CONTACT,
        NEIGHBOURHOOD_CONTACT,
        INTER_NEIGHBOURHOOD_CONTACT,
        IMPORTED_CASE,
        UNDEFINED,
        TRANSITION_REASONS_COUNT
    };
    static constexpr const char *transition_reason_text[] = {
        "HOME_CONTACT",
        "SCHOOL_CONTACT",
        "WORK_CONTACT",
        "NEIGHBOURHOOD_CONTACT",
        "INTER_NEIGHBOURHOOD_CONTACT",
        "IMPORTED_CASE",
        "UNDEFINED"
    };

    struct Delta{
        const PersonState src, dst;
        const TransitionReason reason;
        const vector<PersonId> lst;
        Delta(const PersonState src, const PersonState dst, const TransitionReason reason, const vector<PersonId> lst): src(src), dst(dst), reason(reason), lst(lst) {}

        void apply(SeirState &state, int *transitions_count) const {
            for(const PersonId id: lst){
                if(state.get_estado_persona(id)==src){
                    state.change_state(id, dst);
                    transitions_count[static_cast<int>(reason)]++;
                }
            }
        }
    };
    int transitions_count[TRANSITION_REASONS_COUNT];
    SeirState state;
    const DiseaseParameters disease;
    mt19937 generator;
    vector<Delta> deltas;
    mutex deltas_mutex;

    vector<PersonId> pick_with_probability(const vector<PersonId> &group, const double prob){
        if(prob<1e-9){
            return vector<PersonId>();
        }
        vector<PersonId> delta;
        binomial_distribution<int> distribution(group.size(), min(1., prob));
        sample(begin(group), end(group), back_inserter(delta), distribution(generator), generator);
        return delta;
    }

    vector<PersonId> pick_uniform(const vector<PersonId> &group, const unsigned count){
        if(!count){
            return vector<PersonId>();
        }
        vector<PersonId> delta;
        sample(begin(group), end(group), back_inserter(delta), count, generator);
        return delta;
    }

    void introduce_new_cases_step(int day){
        if(day<=16){
            int new_cases = ceil(disease.initial_new_cases * pow(disease.new_cases_rate, day));

            const int initial_age=25;
            const int final_age=40;
            const int age_range=final_age-initial_age+1;
            for(int age=initial_age; age<final_age; age++){
                add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, IMPORTED_CASE, pick_uniform(state.general[SUSCEPTIBLE][age], ceil(new_cases/static_cast<double>(age_range)))));
            }
        }
    }
    void home_contact_step(){
        for(const auto& env_st: state.environments[HOME]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, HOME_CONTACT, pick_with_probability(env_st.susceptibles, 0.1*env_st.num[INFECTED_1])));
        }
    }
    void school_contact_step(){
        for(const auto& env_st: state.environments[SCHOOL]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, SCHOOL_CONTACT, pick_with_probability(env_st.susceptibles, 0.01*env_st.num[INFECTED_1])));
        }
    }
    void neighbourhood_contact_step(){
        for(const auto& env_st: state.environments[NEIGHBOURHOOD]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, NEIGHBOURHOOD_CONTACT, pick_with_probability(env_st.susceptibles, 0.001*env_st.num[INFECTED_1])));
        }
    }
    void inter_neighbourhood_contact_step(){
        for(unsigned i=0; i<state.population.num_zones; i++){
            auto &env_st = state.environments[NEIGHBOURHOOD][i];
            for(const auto j: state.population.nearests_zones[i]){
                auto &env_st2 = state.environments[NEIGHBOURHOOD][j];
                assert(i!=j);
                add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, INTER_NEIGHBOURHOOD_CONTACT, pick_with_probability(env_st.susceptibles, 0.00001*env_st2.num[INFECTED_1])));
            }
        }
    }

    void add_delta_safe(const Delta& delta){
        if(!delta.lst.empty()){
            lock_guard<mutex> lk(deltas_mutex);
            deltas.push_back(delta);
        }
    }

    void cases_evolution_step(int age){
        add_delta_safe(Delta(EXPOSED, INFECTED_1, UNDEFINED, pick_with_probability(state.general[EXPOSED][age], disease.fraction_become_mild)));
        
        add_delta_safe(Delta(INFECTED_1, RECOVERED, UNDEFINED, pick_with_probability(state.general[INFECTED_1][age], disease.fraction_recover_from_mild)));
        add_delta_safe(Delta(INFECTED_1, INFECTED_2, UNDEFINED, pick_with_probability(state.general[INFECTED_1][age], disease.fraction_severe_from_mild)));

        add_delta_safe(Delta(INFECTED_2, RECOVERED, UNDEFINED, pick_with_probability(state.general[INFECTED_2][age], disease.fraction_recover_from_severe)));
        add_delta_safe(Delta(INFECTED_2, INFECTED_3, UNDEFINED, pick_with_probability(state.general[INFECTED_2][age], disease.fraction_critical_from_severe)));

        add_delta_safe(Delta(INFECTED_3, RECOVERED, UNDEFINED, pick_with_probability(state.general[INFECTED_3][age], disease.fraction_recover_from_critical)));
        add_delta_safe(Delta(INFECTED_3, DEAD, UNDEFINED, pick_with_probability(state.general[INFECTED_3][age], disease.fraction_death_from_critical)));
    }

    void make_day_report(unsigned day, unsigned duration, json &report){
        LOG(info) << "Day " << day;
        report["day"].push_back(day);
        report["compute_time_ms"].push_back(duration);
        for(auto st=0; st<PERSON_STATE_COUNT; st++){
            unsigned count = state.count_state(static_cast<PersonState>(st));
            report[person_state_text[st]].push_back(count);
            LOG(info) << person_state_text[st] << ": " << count;
        }
        for(auto tr=0; tr<TRANSITION_REASONS_COUNT; tr++){
            unsigned count = transitions_count[tr];
            report[transition_reason_text[tr]].push_back(count);
            LOG(info) << transition_reason_text[tr] << ": " << count;
        }
        auto alive_count = state.population.people.size()-state.count_state(DEAD);
        LOG(info) << "TOTAL ALIVE: " << alive_count;
        LOG(info) << "TIME TAKEN: " << duration << "ms"; 
    }

    void step_serial(unsigned day){
        deltas.clear();
        introduce_new_cases_step(day);
        home_contact_step();
        school_contact_step();
        neighbourhood_contact_step();
        inter_neighbourhood_contact_step();
        for(int age=0; age<=MAX_AGE; age++){
            cases_evolution_step(age);
        }
        for(const Delta& d: deltas){
            d.apply(state, transitions_count);
        }
    }
    void step_parallel(unsigned day){
        deltas.clear();
        vector<thread> steps;
        steps.push_back(thread(&SeirSimulation::introduce_new_cases_step, this, day));
        steps.push_back(thread(&SeirSimulation::home_contact_step, this));
        steps.push_back(thread(&SeirSimulation::school_contact_step, this));
        steps.push_back(thread(&SeirSimulation::neighbourhood_contact_step, this));
        steps.push_back(thread(&SeirSimulation::inter_neighbourhood_contact_step, this));
        for(int age=0; age<=MAX_AGE; age++){
            steps.push_back(thread(&SeirSimulation::cases_evolution_step, this, age));
        }
        for (auto& t: steps){
            t.join();
        }
        for(const Delta& d: deltas){
            d.apply(state, transitions_count);
        }
    }
    json create_empty_report(){
        json j;
        j["day"] = vector<int>();
        j["compute_time_ms"] = vector<int>();
        for(int i=0; i<PERSON_STATE_COUNT; i++){
            j[person_state_text[i]]=vector<int>();
        }
        for(auto tr=0; tr<TRANSITION_REASONS_COUNT; tr++){
            j[transition_reason_text[tr]]=vector<int>();
        }
        return j;
    }

public:
    SeirSimulation(Population population, const DiseaseParameters &disease, const unsigned int seed=0): state(population), disease(disease), generator(seed? seed : random_device{}()) {}

    void run(unsigned days, const string &json_filename){
        state.reset();
        json report = create_empty_report();
        LOG(info) << "Starting simulation...";
        for(unsigned day = 1; day<=days; day++){
            auto start = high_resolution_clock::now(); 
            memset(transitions_count, 0, sizeof(transitions_count));
#ifdef DEBUG
            step_serial(day);
#else
            step_serial(day); //TODO: fix parallel (create generator per thread)
#endif
            auto stop = high_resolution_clock::now(); 
            auto duration = duration_cast<milliseconds>(stop - start); 
            make_day_report(day, duration.count(), report);
        }
        ofstream fout(json_filename);
        fout << report;
    }
};
#endif
