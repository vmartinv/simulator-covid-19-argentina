#ifndef SEIR_SIMULATION_HPP
#define SEIR_SIMULATION_HPP
#include <iostream>
#include <random>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <boost/log/trivial.hpp>
#include "endian.hpp"
#include "progress_bar.hpp"
#include "seir_state.hpp"
using namespace std;

#define LOG(severity) BOOST_LOG_TRIVIAL(severity)
#define DEBUG

class SeirSimulation{
    mt19937 generator = mt19937{random_device{}()};

    vector<PersonId> get_delta(const vector<PersonId> &group, const double prob){
        vector<PersonId> delta;
        binomial_distribution<int> distribution(group.size(), prob);
        sample(begin(group), end(group), back_inserter(delta), distribution(generator), generator);
        return delta;
    }
    SeirState state;
public:
    SeirSimulation(Population population): state(population) {
        generator.seed(time(NULL));
    }
    void run(int days=256){
        state.reset();
        LOG(info) << "Starting simulation...";
        for(int day = 1; day<=days; day++){
            for(int age=0; age<=MAX_AGE; age++){
                vector<PersonId> deaths=get_delta(state.general[SUSCEPTIBLE][age], 0.1);
                for(const PersonId id: deaths){
                    state.change_state(id, DEAD);
                }
            }
            auto alive_count = 0;
            for(auto st=0; st<PERSON_STATE_COUNT; st++){
                if (st!=DEAD){
                    for(auto age=0; age<=MAX_AGE; age++){
                        alive_count += state.general[st][age].size();
                    }
                }
            }
            if(alive_count){
                LOG(info) << "Day " << day;
                LOG(info) << "People alive: " << alive_count;
            }
        }  
    }
};
#endif
