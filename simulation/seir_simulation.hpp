#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <random>
#include <algorithm>
#include <vector>
#include "progress_bar.hpp"
#include "seir_state.hpp"
using namespace std;

#define DEBUG

class SeirSimulation{
    SeirState state;
    mt19937 generator;

    vector<PersonId> take_with_prob(const vector<PersonId> &group, const double prob){
        vector<PersonId> delta;
        binomial_distribution<int> distribution(group.size(), prob);
        sample(begin(group), end(group), back_inserter(delta), distribution(generator), generator);
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
        for(int day = 1; day<=days; day++){
            vector<Delta> deltas;
            for(int age=0; age<=MAX_AGE; age++){
                deltas.push_back(Delta(SUSCEPTIBLE, DEAD, take_with_prob(state.general[SUSCEPTIBLE][age], 0.1)));
            }
            for(const Delta& d: deltas){
                d.apply(state);
            }
            auto alive_count = state.count_alive();
            if(alive_count){
                LOG(info) << "Day " << day;
                LOG(info) << "People alive: " << alive_count;
            }
        }  
    }
};
#endif
