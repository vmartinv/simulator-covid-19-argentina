#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
#include <thread>
#include <chrono>
#include "progress_bar.hpp"
#include "seir_state.hpp"
using namespace std;
using namespace std::chrono; 

class SeirSimulation{
    SeirState state;
    mt19937 generator;

    // https://github.com/midas-network/COVID-19/tree/master/parameter_estimates/2019_novel_coronavirus
    const double incubation_period=5.1;  //Incubation period, days
    const double duration_mild_infection=10; //Duration of mild infections, days
    const double fraction_mild=0.8;  //Fraction of infections that are mild
    const double fraction_severe=0.15; //Fraction of infections that are severe
    const double fraction_critical=0.05; //Fraction of infections that are critical
    const double CFR=0.02; //Case fatality rate (fraction of infections resulting in death)
    const double time_ICU_death=7; //Time from ICU admission to death, days
    const double duration_hospitalization=11; //Duration of hospitalization, days
    
    const double fraction_become_mild = 1/incubation_period;
    const double fraction_recover_from_mild = 1/duration_mild_infection * fraction_mild;
    const double fraction_severe_from_mild = 1/duration_mild_infection - fraction_recover_from_mild;

    const double fraction_recover_from_severe = 1/duration_hospitalization * (fraction_critical / (fraction_critical+fraction_severe));
    const double fraction_critical_from_severe = 1/duration_hospitalization - fraction_recover_from_severe;

    const double fraction_recover_from_critical = 1/time_ICU_death * (CFR/fraction_critical);
    const double fraction_death_from_critical = 1/time_ICU_death - fraction_recover_from_critical;

    const double initial_new_cases = 10;
    const double new_cases_rate = pow(2, 1/5.);

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

    struct Delta{
        const PersonState src, dst;
        const vector<PersonId> lst;
        Delta(const PersonState src, const PersonState dst, const vector<PersonId> lst): src(src), dst(dst), lst(lst) {}

        void apply(SeirState &state) const {
            for(const PersonId id: lst){
                if(state.get_estado_persona(id)==src){
                    state.change_state(id, dst);
                }
            }
        }
    };

    void introduce_new_cases_step(int day){
        if(day<=16){
            int new_cases = ceil(initial_new_cases * pow(new_cases_rate, day));

            const int initial_age=25;
            const int final_age=40;
            const int age_range=final_age-initial_age+1;
            for(int age=initial_age; age<final_age; age++){
                add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, pick_uniform(state.general[SUSCEPTIBLE][age], ceil(new_cases/static_cast<double>(age_range)))));
            }
        }
    }
    void home_contact_step(){
        for(const auto& env_st: state.environments[HOME]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.01*env_st.num[INFECTED_1])));
        }
    }
    void school_contact_step(){
        for(const auto& env_st: state.environments[SCHOOL]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.001*env_st.num[INFECTED_1])));
        }
    }
    void neighbourhood_contact_step(){
        for(const auto& env_st: state.environments[NEIGHBOURHOOD]){
            add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.0001*env_st.num[INFECTED_1])));
        }
    }
    void inter_neighbourhood_contact_step(){
        for(unsigned i=0; i<state.population.num_zones; i++){
            auto &env_st = state.environments[NEIGHBOURHOOD][i];
            for(const auto j: state.population.nearests_zones[i]){
                auto &env_st2 = state.environments[NEIGHBOURHOOD][j];
                assert(i!=j);
                add_delta_safe(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.0001*env_st2.num[INFECTED_1])));
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
        add_delta_safe(Delta(EXPOSED, INFECTED_1, pick_with_probability(state.general[EXPOSED][age], fraction_become_mild)));
        
        add_delta_safe(Delta(INFECTED_1, RECOVERED, pick_with_probability(state.general[INFECTED_1][age], fraction_recover_from_mild)));
        add_delta_safe(Delta(INFECTED_1, INFECTED_2, pick_with_probability(state.general[INFECTED_1][age], fraction_severe_from_mild)));

        add_delta_safe(Delta(INFECTED_2, RECOVERED, pick_with_probability(state.general[INFECTED_2][age], fraction_recover_from_severe)));
        add_delta_safe(Delta(INFECTED_2, INFECTED_3, pick_with_probability(state.general[INFECTED_2][age], fraction_critical_from_severe)));

        add_delta_safe(Delta(INFECTED_3, RECOVERED, pick_with_probability(state.general[INFECTED_3][age], fraction_recover_from_critical)));
        add_delta_safe(Delta(INFECTED_3, DEAD, pick_with_probability(state.general[INFECTED_3][age], fraction_death_from_critical)));
    }

    void report(int day){
        LOG(info) << "Day " << day;
        for(auto st=0; st<PERSON_STATE_COUNT; st++){
            LOG(info) << person_state_text[st] << ": " << state.count_state(static_cast<PersonState>(st));
        }
        auto alive_count = state.population.people.size()-state.count_state(DEAD);
        LOG(info) << "TOTAL ALIVE: " << alive_count;
    }

    void step_serial(int day){
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
            d.apply(state);
        }
    }
    void step_parallel(int day){
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
            d.apply(state);
        }
    }

    vector<Delta> deltas;
    mutex deltas_mutex;

public:
    SeirSimulation(Population population, const unsigned int seed=random_device{}()): state(population), generator(seed) {}

    void run(int days=numeric_limits<int>::max()){
        state.reset();
        LOG(info) << "Starting simulation...";
        for(int day = 1; day<=days; day++){
            auto start = high_resolution_clock::now(); 
            report(day);
#ifdef DEBUG
            step_serial(day);
#else
            step_serial(day); //TODO: fix parallel (create generator per thread)
#endif
            auto stop = high_resolution_clock::now(); 
            auto duration = duration_cast<milliseconds>(stop - start); 
            LOG(info) << "TIME TAKEN: " << duration.count() << "ms"; 
        }  
    }
};
#endif
