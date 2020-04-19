#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
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
        INTER_COUNTRY_CONTACT,
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
        "INTER_COUNTRY_CONTACT",
        "IMPORTED_CASE",
        "UNDEFINED"
    };


    struct Delta{
        const PersonState src, dst;
        const TransitionReason reason;
        const vector<PersonId> lst;
        Delta(const PersonState src, const PersonState dst, const TransitionReason reason, const vector<PersonId> lst): src(src), dst(dst), reason(reason), lst(lst) {}

        void apply(SeirState &state, vector<vector<unsigned>> &state_trans_by_zone) const {
            for(const PersonId id: lst){
                if(state.change_state(id, src, dst)){
                    const unsigned zone = state.population.families[state.population.people[id].family].zone;
                    state_trans_by_zone.back()[static_cast<int>(reason)]++;
                    state_trans_by_zone[zone][static_cast<int>(reason)]++;
                }
            }
        }
    };
    vector<vector<unsigned>> state_trans_by_zone;
    SeirState state;
    const DiseaseParameters disease;
    mt19937 generator;
    vector<Delta> deltas;

    void add_delta_safe(const Delta& delta){
        if(!delta.lst.empty()){
            deltas.push_back(delta);
        }
    }

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
            add_delta_safe(Delta(
                SUSCEPTIBLE,
                EXPOSED,
                IMPORTED_CASE,
                pick_uniform(state.get_environments(COUNTRY)[0].people[SUSCEPTIBLE], new_cases)
            ));
        }
    }
    void home_contact_step(){
        for(const auto& env_st: state.get_environments(HOME)){
            add_delta_safe(Delta(
                SUSCEPTIBLE,
                EXPOSED,
                HOME_CONTACT,
                pick_with_probability(env_st.people[SUSCEPTIBLE], 0.1*env_st.people[INFECTED_1].size())
            ));
        }
    }
    void school_contact_step(){
        for(const auto& env_st: state.get_environments(SCHOOL)){
            add_delta_safe(Delta(
                SUSCEPTIBLE,
                EXPOSED,
                SCHOOL_CONTACT,
                pick_with_probability(env_st.people[SUSCEPTIBLE], 0.01*env_st.people[INFECTED_1].size())
            ));
        }
    }

    void neighbourhood_contact_step(){
        for(const auto& env_st: state.get_environments(NEIGHBOURHOOD)){
            add_delta_safe(Delta(
                SUSCEPTIBLE,
                EXPOSED,
                NEIGHBOURHOOD_CONTACT,
                pick_with_probability(env_st.people[SUSCEPTIBLE], 0.001*env_st.people[INFECTED_1].size())
            ));
        }
    }
    void inter_neighbourhood_contact_step(){
        for(unsigned i=0; i<state.population.num_zones; i++){
            auto &env_st = state.get_environments(NEIGHBOURHOOD)[i];
            for(const auto j: state.population.nearests_zones[i]){
                auto &env_st2 = state.get_environments(NEIGHBOURHOOD)[j];
                assert(i!=j);
                add_delta_safe(Delta(
                    SUSCEPTIBLE,
                    EXPOSED,
                    INTER_NEIGHBOURHOOD_CONTACT,
                    pick_with_probability(env_st.people[SUSCEPTIBLE], 0.00001*env_st2.people[INFECTED_1].size())
                ));
            }
        }
    }

    void inter_country_contact_step(){
        auto &env_st = state.get_environments(COUNTRY)[0];
        add_delta_safe(Delta(
            SUSCEPTIBLE,
            EXPOSED,
            INTER_COUNTRY_CONTACT,
            pick_with_probability(env_st.people[SUSCEPTIBLE], 0.0000001*env_st.people[INFECTED_1].size())
        ));
    }

    void cases_evolution_step(){
        for(int age=0; age<=MAX_AGE; age++){
            add_delta_safe(Delta(
                EXPOSED,
                INFECTED_1,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[EXPOSED], disease.fraction_become_mild)
            ));

            add_delta_safe(Delta(
                INFECTED_1,
                RECOVERED,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_1], disease.fraction_recover_from_mild)
            ));
            add_delta_safe(Delta(
                INFECTED_1,
                INFECTED_2,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_1], disease.fraction_severe_from_mild)
            ));

            add_delta_safe(Delta(
                INFECTED_2,
                RECOVERED,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_2], disease.fraction_recover_from_severe)
            ));
            add_delta_safe(Delta(
                INFECTED_2,
                INFECTED_3,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_2], disease.fraction_critical_from_severe)
            ));

            add_delta_safe(Delta(
                INFECTED_3,
                RECOVERED,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_3], disease.fraction_recover_from_critical)
            ));
            add_delta_safe(Delta(
                INFECTED_3,
                DEAD,
                UNDEFINED,
                pick_with_probability(state.get_environments(BY_AGE)[age].people[INFECTED_3], disease.fraction_death_from_critical)
            ));
        }
    }

    void run_steps(unsigned day){
        deltas.clear();
        introduce_new_cases_step(day);
        home_contact_step();
        school_contact_step();
        neighbourhood_contact_step();
        inter_neighbourhood_contact_step();
        inter_country_contact_step();
        cases_evolution_step();
        for(const Delta& d: deltas){
            d.apply(state, state_trans_by_zone);
        }
    }

    json create_empty_report(){
        json j;
        j["general"]["compute_time_ms"] = json::array();
        for(const auto& context: {"by_zone","general"}){
            j[context]["day"] = json::array();
            for(int i=0; i<PERSON_STATE_COUNT; i++){
                j[context][person_state_text[i]]=json::array();
            }
            for(auto tr=0; tr<TRANSITION_REASONS_COUNT; tr++){
                j[context][transition_reason_text[tr]]=json::array();
            }
        }
        return j;
    }

    void make_day_report(unsigned day, unsigned duration, json &report){
        LOG(info) << "Day " << day;
        report["general"]["day"].push_back(day);
        report["general"]["compute_time_ms"].push_back(duration);
        for(auto st=0; st<PERSON_STATE_COUNT; st++){
            const unsigned count = state.count_state(static_cast<PersonState>(st));
            report["general"][person_state_text[st]].push_back(count);
            LOG(info) << person_state_text[st] << ": " << count;
        }
        for(auto tr=0; tr<TRANSITION_REASONS_COUNT; tr++){
            const unsigned count = state_trans_by_zone.back()[tr];
            report["general"][transition_reason_text[tr]].push_back(count);
            LOG(info) << transition_reason_text[tr] << ": " << count;
        }

        vector<vector<int>> state_count_by_zone(state.population.num_zones, vector<int>(PERSON_STATE_COUNT));
        const auto &psv = state.get_person_states();
        for(unsigned id=0; id<psv.size(); id++){
            const unsigned zone = state.population.families[state.population.people[id].family].zone;
            state_count_by_zone[zone][psv[id].state]++;
        }
        for(unsigned zone=0; zone<state.population.num_zones; zone++){
            report["by_zone"]["day"].push_back(day);
            report["by_zone"]["zone"].push_back(zone);
            for(auto st=0; st<PERSON_STATE_COUNT; st++){
                report["by_zone"][person_state_text[st]].push_back(state_count_by_zone[zone][st]);
            }
            for(auto tr=0; tr<TRANSITION_REASONS_COUNT; tr++){
                const unsigned count = state_trans_by_zone[zone][tr];
                report["by_zone"][transition_reason_text[tr]].push_back(count);
            }
        }
        const auto alive_count = state.population.people.size()-state.count_state(DEAD);
        LOG(info) << "TOTAL ALIVE: " << alive_count;
        LOG(info) << "TIME TAKEN: " << duration << "ms";
    }

public:
    SeirSimulation(Population population, const DiseaseParameters &disease, const unsigned int seed=0): state(population), disease(disease), generator(seed? seed : random_device{}()) {
        assert(TRANSITION_REASONS_COUNT == sizeof(transition_reason_text)/sizeof(transition_reason_text[0]));
    }

    void run(unsigned days, const string &json_filename, const ProgressBar::ShowMode progress_mode=ProgressBar::DEFAULT){
        state.reset();
        json report = create_empty_report();
        LOG(info) << "Starting simulation...";
        ProgressBar progressBar(days, progress_mode);
        for(unsigned day = 1; day<=days; day++){
            auto start = high_resolution_clock::now();
            state_trans_by_zone = vector<vector<unsigned>>(state.population.num_zones+1, vector<unsigned>(TRANSITION_REASONS_COUNT));
            //TODO: add parallel (create generator per thread)
            run_steps(day);
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(end - start);
            make_day_report(day, duration.count(), report);
            ++progressBar;
        }
        progressBar.done();
        LOG(info) << "Saving report...";
        ofstream(json_filename) << report;
    }
};
#endif
