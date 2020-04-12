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
    unsigned short zone;
    unsigned char edad;
    bool sexo;
    unsigned int escuela, trabajo;
};

ostream& operator<< (ostream& os, const Person& p)
{
    os << p.id << ' ';
    os << p.family << ' ';
    os << p.zone << ' ';
    os << int(p.edad) << ' ';
    os << p.sexo << ' ';
    os << p.escuela << ' ';
    os << p.trabajo;
    return os;
}

istream& operator>> (istream& is, Person& p)
{
    static const size_t struct_size = 20;
    unsigned char buffer[struct_size];
    is.read(reinterpret_cast<char*>(buffer), struct_size);
    unsigned char *write_ptr=buffer;
    p.id = from_big_endian<uint32_t>(write_ptr);
    write_ptr+=4;
    p.family = from_big_endian<uint32_t>(write_ptr);
    write_ptr+=4;
    p.zone = from_big_endian<uint16_t>(write_ptr);
    write_ptr+=2;
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

const int MAX_AGE = 110;
typedef unsigned PersonId;
typedef unsigned ZoneId;

class Population{
private:
    void load_json(const string &json_filename){    
        LOG(info) << "Loading database " << json_filename << "...";
        ifstream file(json_filename);
        json j;
        file >> j;
        nearests_zones = vector<vector<ZoneId>>(j["nearest_zones"]);
    }
public:
    vector<Person> people;
    vector<vector<ZoneId>> nearests_zones;
    unsigned num_families = 0;
    unsigned num_zones = 0;
    unsigned num_schools = 0;
    unsigned char max_age = 0;
    static const unsigned NO_SCHOOL = 0;
    Population() {}
    Population(const string &pop_filename, const string &json_filename){
        LOG(info) << "Loading database " << pop_filename << "...";
        ifstream file(pop_filename, ios::binary);
        std::copy(std::istream_iterator<Person>(file),
            std::istream_iterator<Person>(),
            std::back_inserter(people));
        assert(people.size() > 0);
        num_families = people.back().family+1;
        num_zones = people.back().zone+1;
        num_schools = max_element(begin(people), end(people),
        [] (Person const& s1, Person const& s2) { return s1.escuela < s2.escuela; })->escuela+1;
        max_age = max_element(begin(people), end(people),
        [] (Person const& s1, Person const& s2) { return s1.edad < s2.edad; })->edad;
        load_json(json_filename);
        validate();
        report();
    }

    void validate() const{
        LOG(info) << "Validating database...";
        assert(num_zones==nearests_zones.size());
        ProgressBar progressBar(people.size(), 70);
        for(const auto &p : people){
            assert(0 <= p.id && p.id < people.size());
            assert(0 <= p.family && p.family < num_families);
            ++progressBar;
        }
        progressBar.done();
    }

    inline const Person &get(const int idx) const {
        return people[idx];
    }

    void report() const {
        LOG(info) << "Number of people: " << people.size();
        LOG(info) << "Number of families: " << num_families;
        LOG(info) << "Number of zones: " << num_zones;
        LOG(info) << "Number of schools: " << num_schools;
        LOG(info) << "Random person: " << people[rand() % people.size()];
    }
};

#endif
