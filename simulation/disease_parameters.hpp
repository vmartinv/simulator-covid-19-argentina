#ifndef DISEASE_PARAMETERS_HPP
#define DISEASE_PARAMETERS_HPP

class DiseaseParameters {
public:
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
};

#endif //DISEASE_PARAMETERS_HPP
