#ifndef POPULATION_HPP
#define POPULATION_HPP
#include <boost/range/adaptor/reversed.hpp>
#include "common.hpp"
#include "endian.hpp"
#include "progress_bar.hpp"
using namespace std;


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

class Population{
public:
    vector<Person> people;
    unsigned num_families = 0;
    unsigned num_zones = 0;
    unsigned num_schools = 0;
    unsigned char max_age = 0;
    const unsigned NO_SCHOOL = 0;
    Population() {}
    Population(const string &pop_filename){
        LOG(info) << "Loading database " << pop_filename << "...";    
        ifstream file(pop_filename, ios::binary);
        std::copy(std::istream_iterator<Person>(file),
            std::istream_iterator<Person>(),
            std::back_inserter(people));
        assert(people.size() > 0);
        num_families = people.back().family+1;
        num_zones = people.back().zone+1;
        for (auto p : boost::adaptors::reverse(people)){
            if(p.escuela!=NO_SCHOOL){
                num_schools = p.escuela+1;
                break;
            }
        }
        max_age = max_element(begin(people), end(people),
        [] (Person const& s1, Person const& s2) { return s1.edad < s2.edad; })->edad;
        validate();
        report();
    }

    void validate(){
        LOG(info) << "Validating database...";
        ProgressBar progressBar(people.size(), 70);
        for(const auto &p : people){
            assert(0 <= p.id && p.id < people.size());
            assert(0 <= p.family && p.family < num_families);
            ++progressBar;
        }
        progressBar.done();
    }

    inline Person &get(const int idx) {
        return people[idx];
    }

    void report(){
        LOG(info) << "Number of people: " << people.size();
        LOG(info) << "Random person: " << people[rand() % people.size()];
    }
};

#endif
