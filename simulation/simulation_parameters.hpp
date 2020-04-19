#ifndef SIMULATION_PARAMETERS_HPP
#define SIMULATION_PARAMETERS_HPP

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class SimulationParameters {
public:
    SimulationParameters(const json &_data=json()): data(_data){
        map<string, double> defaults;
        // https://github.com/midas-network/COVID-19/tree/master/parameter_estimates/2019_novel_coronavirus
        defaults["incubation_period"] = 5.1;
        defaults["duration_mild_infection"] = 10;
        defaults["fraction_mild"] = 0.8;
        defaults["fraction_severe"] = 0.05;
        defaults["fraction_critical"] = 0.02;
        defaults["CFR"] = 0.02;
        defaults["time_ICU_death"] = 7;
        defaults["duration_hospitalization"] = 11;
        defaults["initial_new_cases"] = 10;
        defaults["new_cases_rate"] = pow(2, 1/5.);
        defaults["home_contact_probability"] = 0.1;
        defaults["school_contact_probability"] = defaults["home_contact_probability"]/100;
        defaults["neighbourhood_contact_probability"] = defaults["home_contact_probability"]/1000;
        defaults["inter_province_contact_probability"] = defaults["home_contact_probability"]/10000000;
        for(const auto& def: defaults){
            if(!data.count(def.first)){
                data[def.first] = def.second;
            }
        }
        if(!data.count("fraction_become_mild")){
            data["fraction_become_mild"] = 1/data["incubation_period"].get<double>();
        }
        if(!data.count("fraction_recover_from_mild")){
            data["fraction_recover_from_mild"] = 1/data["duration_mild_infection"].get<double>() * data["fraction_mild"].get<double>();
        }
        if(!data.count("fraction_severe_from_mild")){
            data["fraction_severe_from_mild"] = 1/data["duration_mild_infection"].get<double>() - data["fraction_recover_from_mild"].get<double>();
        }

        if(!data.count("fraction_recover_from_severe")){
            data["fraction_recover_from_severe"] = 1/data["duration_hospitalization"].get<double>() * (data["fraction_critical"].get<double>() / (data["fraction_critical"].get<double>()+data["fraction_severe"].get<double>()));
        }
        if(!data.count("fraction_critical_from_severe")){
            data["fraction_critical_from_severe"] = 1/data["duration_hospitalization"].get<double>() - data["fraction_recover_from_severe"].get<double>();
        }

        if(!data.count("fraction_recover_from_critical")){
            data["fraction_recover_from_critical"] = 1/data["time_ICU_death"].get<double>() * (data["CFR"].get<double>()/data["fraction_critical"].get<double>());
        }
        if(!data.count("fraction_death_from_critical")){
            data["fraction_death_from_critical"] = 1/data["time_ICU_death"].get<double>() - data["fraction_recover_from_critical"].get<double>();
        }
    }


    const auto &operator[](const string &name) const{
        fail_if(!data.count(name), "Missing simulation parameter: "+name);
        return data[name];
    }
private:
    json data;
};

#endif //SIMULATION_PARAMETERS_HPP
