#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
#include "progress_bar.hpp"
#include "seir_state.hpp"
using namespace std;

class SeirSimulation{
    SeirState state;
    mt19937 generator;

    vector<PersonId> pick_with_probability(const vector<PersonId> &group, const double prob){
        if(prob<1e-9){
            return vector<PersonId>();
        }
        vector<PersonId> delta;
        binomial_distribution<int> distribution(group.size(), prob);
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
                state.change_state(id, dst);
            }
        }
    };

public:
    SeirSimulation(Population population, const unsigned int seed=random_device{}()): state(population), generator(seed) {}
    void run(int days=256){
        state.reset();
        LOG(info) << "Starting simulation...";
        // https://github.com/midas-network/COVID-19/tree/master/parameter_estimates/2019_novel_coronavirus
        double incubation_period=5.1;  //Incubation period, days
        double duration_mild_infection=10; //Duration of mild infections, days
        double fraction_mild=0.8;  //Fraction of infections that are mild
        double fraction_severe=0.15; //Fraction of infections that are severe
        double fraction_critical=0.05; //Fraction of infections that are critical
        double CFR=0.02; //Case fatality rate (fraction of infections resulting in death)
        double time_ICU_death=7; //Time from ICU admission to death, days
        double duration_hospitalization=11; //Duration of hospitalization, days
        
        double fraction_become_mild = 1/incubation_period;
        double fraction_recover_from_mild = 1/duration_mild_infection * fraction_mild;
        double fraction_severe_from_mild = 1/duration_mild_infection - fraction_recover_from_mild;

        double fraction_recover_from_severe = 1/duration_hospitalization * (fraction_critical / (fraction_critical+fraction_severe));
        double fraction_critical_from_severe = 1/duration_hospitalization - fraction_recover_from_severe;

        double fraction_recover_from_critical = 1/time_ICU_death * (CFR/fraction_critical);
        double fraction_death_from_critical = 1/time_ICU_death - fraction_recover_from_critical;

        double initial_new_cases = 10;
        double new_cases_rate = pow(2, 1/5.);
        for(int day = 1; day<=days; day++){
            LOG(info) << "Day " << day;
            for(auto st=0; st<PERSON_STATE_COUNT; st++){
                LOG(info) << person_state_text[st] << ": " << state.count_state(static_cast<PersonState>(st));
            }
            auto alive_count = state.population.people.size()-state.count_state(DEAD);
            LOG(info) << "TOTAL ALIVE: " << alive_count;

            vector<Delta> deltas;
            if(day<=10){
                int new_cases = ceil(initial_new_cases * pow(new_cases_rate, day));

                int initial_age=25;
                int final_age=40;
                int age_range=final_age-initial_age+1;
                for(int age=initial_age; age<final_age; age++){
                    deltas.push_back(Delta(SUSCEPTIBLE, EXPOSED, pick_uniform(state.general[SUSCEPTIBLE][age], ceil(new_cases/static_cast<double>(age_range)))));
                }
            }

            for(const auto& env_st: state.environments[HOME]){
                deltas.push_back(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.01*env_st.num[INFECTED_1])));
            }
            for(const auto& env_st: state.environments[SCHOOL]){
                deltas.push_back(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.001*env_st.num[INFECTED_1])));
            }
            for(const auto& env_st: state.environments[NEIGHBOURHOOD]){
                deltas.push_back(Delta(SUSCEPTIBLE, EXPOSED, pick_with_probability(env_st.susceptibles, 0.0001*env_st.num[INFECTED_1])));
            }
            for(int age=0; age<=MAX_AGE; age++){
                deltas.push_back(Delta(EXPOSED, INFECTED_1, pick_with_probability(state.general[EXPOSED][age], fraction_become_mild)));
                deltas.push_back(Delta(INFECTED_1, RECOVERED, pick_with_probability(state.general[INFECTED_1][age], fraction_recover_from_mild)));
                deltas.push_back(Delta(INFECTED_1, INFECTED_2, pick_with_probability(state.general[INFECTED_1][age], fraction_severe_from_mild)));

                deltas.push_back(Delta(INFECTED_2, RECOVERED, pick_with_probability(state.general[INFECTED_2][age], fraction_recover_from_severe)));
                deltas.push_back(Delta(INFECTED_2, INFECTED_3, pick_with_probability(state.general[INFECTED_2][age], fraction_critical_from_severe)));

                deltas.push_back(Delta(INFECTED_3, RECOVERED, pick_with_probability(state.general[INFECTED_3][age], fraction_recover_from_critical)));
                deltas.push_back(Delta(INFECTED_3, DEAD, pick_with_probability(state.general[INFECTED_3][age], fraction_death_from_critical)));
            }
            for(const Delta& d: deltas){
                d.apply(state);
            }
        }  
    }
};
#endif
