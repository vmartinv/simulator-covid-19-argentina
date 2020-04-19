#ifndef POPULATION_HPP
#define POPULATION_HPP
#include <boost/range/adaptor/reversed.hpp>
#include <nlohmann/json.hpp>
#include "common.hpp"
#include "endian.hpp"
#include "progress_bar.hpp"
using namespace std;
using json = nlohmann::json;


struct Person{
    unsigned int id, family;
    unsigned char edad;
    bool sexo;
    unsigned int escuela, trabajo;
};

ostream& operator<< (ostream& os, const Person& p)
{
    os << p.id << ' ';
    os << p.family << ' ';
    os << int(p.edad) << ' ';
    os << p.sexo << ' ';
    os << p.escuela << ' ';
    os << p.trabajo;
    return os;
}

istream& operator>> (istream& is, Person& p)
{
    static const size_t struct_size = 14;
    unsigned char buffer[struct_size];
    is.read(reinterpret_cast<char*>(buffer), struct_size);
    unsigned char *write_ptr=buffer;
    p.family = from_big_endian<uint32_t>(write_ptr);
    write_ptr+=4;
    p.edad = from_big_endian<unsigned char>(write_ptr);
    write_ptr+=1;
    p.sexo = from_big_endian<unsigned char>(write_ptr);
    write_ptr+=1;
    p.escuela = from_big_endian<uint32_t>(write_ptr);
    write_ptr+=4;
    p.trabajo = from_big_endian<uint32_t>(write_ptr);
    write_ptr+=4;
    return is;
}

struct Family{
    unsigned int id;
    unsigned short zone;
    unsigned int dpto, prov;
};

ostream& operator<< (ostream& os, const Family& p)
{
    os << p.id << ' ';
    os << p.zone << ' ';
    os << p.dpto << ' ';
    os << p.prov << ' ';
    return os;
}

istream& operator>> (istream& is, Family& p)
{
    static const size_t struct_size = 6;
    unsigned char buffer[struct_size];
    is.read(reinterpret_cast<char*>(buffer), struct_size);
    unsigned char *write_ptr=buffer;
    p.zone = from_big_endian<uint16_t>(write_ptr);
    write_ptr+=2;
    p.dpto = from_big_endian<uint16_t>(write_ptr);
    write_ptr+=2;
    p.prov = from_big_endian<uint16_t>(write_ptr);
    write_ptr+=2;
    return is;
}

unsigned int read_uint32(istream &is){
    unsigned char buffer[4];
    is.read(reinterpret_cast<char*>(buffer), 4);
    return from_big_endian<uint32_t>(buffer);;
}

const int MAX_AGE = 110;
typedef unsigned PersonId;
typedef unsigned ZoneId;
typedef unsigned DptoId;
typedef unsigned ProvId;

class Population{
private:
    void load_json(const string &json_filename){
        LOG(info) << "Loading database " << json_filename << "...";
        ifstream file(json_filename);
        fail_if(file.fail(), json_filename + " not found!");
        json j;
        file >> j;
        nearests_zones = vector<vector<ZoneId>>(j["nearest_zones"]);
    }
    void load_pop(const string &pop_filename){
        LOG(info) << "Loading database " << pop_filename << "...";
        ifstream file(pop_filename, ios::binary);
        fail_if(file.fail(), pop_filename + " not found!");
        num_families = read_uint32(file);
        for(unsigned i=0; i<num_families; i++){
            Family family;
            file >> family;
            family.id = i;
            families.push_back(family);
        }
        unsigned num_people = read_uint32(file);
        for(unsigned i=0; i<num_people; i++){
            Person person;
            file >> person;
            person.id = i;
            people.push_back(person);
        }
        fail_if(people.empty(), "No people in dataset!");
        fail_if(families.empty(), "No families in dataset!");
        num_zones = families.back().zone+1;
        num_dptos = families.back().dpto+1;
        num_provs = families.back().prov+1;
        num_schools = max_element(begin(people), end(people),
        [] (Person const& s1, Person const& s2) { return s1.escuela < s2.escuela; })->escuela+1;
        max_age = max_element(begin(people), end(people),
        [] (Person const& s1, Person const& s2) { return s1.edad < s2.edad; })->edad;
    }

    void validate() const {
        LOG(info) << "Validating database...";
        fail_if(num_zones != nearests_zones.size(), "Not all zones have neighbourhood list");
        ProgressBar progressBar(people.size()+families.size(), 70);
        unsigned last_zone=families[0].zone;
        unsigned last_dpto=families[0].dpto;
        unsigned last_prov=families[0].prov;
        for(unsigned i=1; i<families.size(); i++){
            fail_if(families[i].zone-last_zone<0, "Zones not in ascending order");
            fail_if(families[i].zone-last_zone>1, "Zones don't have all the ids");
            last_zone = families[i].zone;

            fail_if(families[i].dpto-last_dpto<0, "Dptos not in ascending order");
            fail_if(families[i].dpto-last_dpto>1, "Dptos don't have all the ids");
            last_dpto = families[i].dpto;

            fail_if(families[i].prov-last_prov<0, "Provs not in ascending order");
            fail_if(families[i].prov-last_prov>1, "Provs don't have all the ids");
            last_prov = families[i].prov;
            ++progressBar;
        }
        unsigned last_family=people[0].family;
        for(unsigned i=1; i<people.size(); i++){
            fail_if(people[i].family-last_family<0, "Families not in ascending order");
            fail_if(people[i].family-last_family>1, "Families don't have all the ids");
            last_family = people[i].family;

            fail_if(!(0 <= people[i].escuela && people[i].escuela < num_schools), "Schools not in [0..n] range");
            ++progressBar;
        }
        fail_if(num_families != people.back().family+1, "some families are empty");
        progressBar.done();
    }
public:
    vector<Person> people;
    vector<Family> families;
    vector<vector<ZoneId>> nearests_zones;
    unsigned num_families = 0;
    unsigned num_schools = 0;
    unsigned num_zones = 0;
    unsigned num_dptos = 0;
    unsigned num_provs = 0;
    unsigned char max_age = 0;
    static const unsigned NO_SCHOOL = 0;
    Population() {}
    Population(const string &pop_filename, const string &json_filename){
        load_pop(pop_filename);
        load_json(json_filename);
        report();
        validate();
    }

    inline const Person &get(const int idx) const {
        return people[idx];
    }

    void report() const {
        LOG(info) << "Number of people: " << people.size();
        LOG(info) << "Number of families: " << num_families;
        LOG(info) << "Number of schools: " << num_schools;
        LOG(info) << "Number of zones: " << num_zones;
        LOG(info) << "Number of dptos: " << num_dptos;
        LOG(info) << "Number of provs: " << num_provs;
        LOG(info) << "First family: " << families[0];
        LOG(info) << "Second family: " << families[1];
        LOG(info) << "First person: " << people[0];
        LOG(info) << "Second person: " << people[1];
        LOG(info) << "Random family: " << families[rand() % families.size()];
        LOG(info) << "Random person: " << people[rand() % people.size()];
    }
};

#endif
