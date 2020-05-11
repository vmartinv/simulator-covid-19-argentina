#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include "progress_bar.hpp"
#include "simulation_parameters.hpp"
#include "seir_state.hpp"
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;

template <class T>
class CircularDayQueue{
    const size_t capacity;
    vector<vector<T>> queue;
    size_t current_start=0;
public:
    CircularDayQueue(const size_t capacity=100): capacity(capacity), queue(capacity) {}

    void clear(){
        current_start = 0;
        for(auto& v: queue){
            v.clear();
        }
    }
    void push(const T& e, size_t days_from_now){
        queue[(days_from_now+current_start)%capacity].push_back(e);
    }

    vector<T> pop(){
        auto ret = move(queue[current_start]);
        current_start = (current_start+1)%capacity;
        return ret;
    }
};

class SeirSimulation{
    enum TransitionReason{
        HOME_CONTACT = 0,
        SCHOOL_CONTACT,
        WORK_CONTACT,
        NEIGHBOURHOOD_CONTACT,
        INTER_PROVINCE_CONTACT,
        IMPORTED_CASE,
        UNDEFINED,
        TRANSITION_REASONS_COUNT
    };
    static constexpr const char *transition_reason_text[] = {
        "HOME_CONTACT",
        "SCHOOL_CONTACT",
        "WORK_CONTACT",
        "NEIGHBOURHOOD_CONTACT",
        "INTER_PROVINCE_CONTACT",
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
    CircularDayQueue<Delta> future_transitions;
    uniform_real_distribution<> random_probability;
    const SimulationParameters parameters;
    mt19937 generator;
    vector<Delta> deltas;

    void infect(const TransitionReason &tr, const vector<PersonId> &lst){
        if(!lst.empty()){
            deltas.push_back(Delta(
                SUSCEPTIBLE,
                EXPOSED,
                tr,
                lst
            ));
            prepare_future_transitions(EXPOSED, lst);
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

    vector<PersonId> stable_difference(const vector<PersonId> &group, const vector<PersonId> &subset){
        vector<PersonId> ret;
        auto it = begin(subset);
        for(const auto &p: group){
            if(it==end(subset) || p!=*it){
                ret.push_back(p);
            }
            else{
                ++it;
            }
        }
        return ret;
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
            int new_cases = ceil(parameters["initial_new_cases"].get<double>() * pow(parameters["new_cases_rate"].get<double>(), day));
            infect(
                IMPORTED_CASE,
                pick_uniform(state.get_environments(COUNTRY)[0].people[SUSCEPTIBLE], new_cases)
            );
        }
    }
    void home_contact_step(){
        const double beta = parameters["home_contact_probability"].get<double>();
        for(const auto& env_st: state.get_environments(HOME)){
            infect(
                HOME_CONTACT,
                pick_with_probability(env_st.people[SUSCEPTIBLE], beta*env_st.people[INFECTED_1].size())
            );
        }
    }
    void school_contact_step(){
        const double beta = parameters["school_contact_probability"].get<double>();
        for(const auto& env_st: state.get_environments(SCHOOL)){
            infect(
                SCHOOL_CONTACT,
                pick_with_probability(env_st.people[SUSCEPTIBLE], beta*env_st.people[INFECTED_1].size())
            );
        }
    }

    void neighbourhood_contact_step(){
        const double beta = parameters["neighbourhood_contact_probability"].get<double>();
        for(unsigned i=0; i<state.population.num_zones; i++){
            const auto &env_st = state.get_environments(NEIGHBOURHOOD)[i];
            const double sqrtdensity = sqrt(state.population.nearest_densities[i]);
            for(const auto j: state.population.nearests_zones[i]){
                const auto &env_st2 = state.get_environments(NEIGHBOURHOOD)[j];
                infect(
                    NEIGHBOURHOOD_CONTACT,
                    pick_with_probability(env_st.people[SUSCEPTIBLE], beta*sqrtdensity*env_st2.people[INFECTED_1].size())
                );
            }
        }
    }

    void inter_province_contact_step(){
        auto &env_st = state.get_environments(COUNTRY)[0];
        infect(
            INTER_PROVINCE_CONTACT,
            pick_with_probability(env_st.people[SUSCEPTIBLE], parameters["inter_province_contact_probability"].get<double>()*env_st.people[INFECTED_1].size())
        );
    }

    void prepare_future_transitions(const PersonState &st, const vector<PersonId> &lst){
        switch(st){
        case EXPOSED: {
            future_transitions.push(Delta(
                    EXPOSED,
                    INFECTED_1,
                    UNDEFINED,
                    lst
            ), round(parameters["incubation_period"].get<double>()));
            break;
        }
        case INFECTED_1: {
            auto recovered = pick_with_probability(lst, parameters["fraction_mild"].get<double>());
            future_transitions.push(Delta(
                    INFECTED_1,
                    RECOVERED,
                    UNDEFINED,
                    recovered
            ), round(parameters["duration_mild_infection"].get<double>()));
            future_transitions.push(Delta(
                    INFECTED_1,
                    INFECTED_2,
                    UNDEFINED,
                    stable_difference(lst, recovered)
            ), round(parameters["duration_mild_infection"].get<double>()));
            break;
        }
        case INFECTED_2: {
            auto recovered2 = pick_with_probability(lst, parameters["fraction_severe"].get<double>());
            future_transitions.push(Delta(
                    INFECTED_2,
                    RECOVERED,
                    UNDEFINED,
                    recovered2
            ), round(parameters["duration_hospitalization"].get<double>()));
            future_transitions.push(Delta(
                    INFECTED_2,
                    INFECTED_3,
                    UNDEFINED,
                    stable_difference(lst, recovered2)
            ), round(parameters["duration_hospitalization"].get<double>()));
            break;
        }
        case INFECTED_3: {
            auto recovered3 = pick_with_probability(lst, parameters["CFR"].get<double>()/parameters["fraction_critical"].get<double>());
            future_transitions.push(Delta(
                    INFECTED_3,
                    RECOVERED,
                    UNDEFINED,
                    recovered3
            ), round(parameters["time_ICU_death"].get<double>()));
            future_transitions.push(Delta(
                    INFECTED_3,
                    DEAD,
                    UNDEFINED,
                    stable_difference(lst, recovered3)
            ), round(parameters["time_ICU_death"].get<double>()));
            break;
        }
        case SUSCEPTIBLE:
        case RECOVERED:
        case DEAD:
        case PERSON_STATE_COUNT:
            break;
        }
    }

    void run_steps(unsigned day){
        deltas.clear();
        for(const Delta& d: future_transitions.pop()){
            d.apply(state, state_trans_by_zone);
            prepare_future_transitions(d.dst, d.lst);
        }
        introduce_new_cases_step(day);
        home_contact_step();
        school_contact_step();
        neighbourhood_contact_step();
        inter_province_contact_step();
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
    SeirSimulation(Population population, const SimulationParameters &parameters, const unsigned int seed=0): state(population), random_probability(0, 1), parameters(parameters), generator(seed? seed : random_device{}()) {
        assert(TRANSITION_REASONS_COUNT == sizeof(transition_reason_text)/sizeof(transition_reason_text[0]));
    }

    void run(unsigned days, const string &json_filename, const ProgressBar::ShowMode progress_mode=ProgressBar::DEFAULT){
        state.reset();
        future_transitions.clear();
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
