#include <cstdio>
#include <fstream>
#include <iostream>
#include <ctime>
#include "json.hpp"
#include <unistd.h>

// for convenience
using json = nlohmann::json;
using namespace std;

class Configuration {
public:
    Configuration()
    {
        restore();
        reagents = &_config["reagents"];
        devices = &_config["devices"];
        virtual_tissue_processors = &_config["virtual_tissue_processors"];
        programs = &_config["programs"];
        run = &_config["run"];
    }

    void restore()
    {
        _config.clear();

        std::ifstream in("configuration.json");
        in >> _config;
        in.close();
    }

public:
    json *reagents;
    json *devices;
    json *virtual_tissue_processors;
    json *programs;
    json *run;

private:
    json _config;
};

class Reagent {
    public:
        Reagent(json &j)
        {

        }

};

struct step {
    step(json &j)
    {
        id = j["id"].get<int>();
        reagent = j["reagent"].get<std::string>();
        duration = j["duration"].get<int>() * 60;
        temperature = j["temperature"].get<int>();
        is_pressure = (j["pressure"].get<std::string>() == "on") ? true : false;
        is_vacuum = (j["vacuum"].get<std::string>() == "on") ? true : false;
    }

    std::string dump()
    {
        return ("id : " + std::to_string(id) + "\nreagent : " + reagent + "\nduration : " + std::to_string(duration) + "\ntemperature : "
            + "\npressure : " + std::to_string(is_pressure) + "\nvacuum : " + std::to_string(is_vacuum));

    }

    int id;
    std::string reagent;
    int duration;
    int temperature;
    bool is_pressure;
    bool is_vacuum;

    int step_time;
};

class Program {
public:
    Program(std::string _name, json &_j, std::string start_time_, Configuration *config_, int retort_)
        : end_time(0), offset(0), index(0), name(_name), config(config_), retort(retort_)
    {
        for (size_t i = 0; i < _j.size(); ++i)
            steps.insert(make_pair(i, new step(_j[i])));
        struct tm tm_ = {0};
        strptime(start_time_.c_str(), "%Y-%m-%d %H:%M:%S", &tm_);
        start_time = mktime(&tm_);
    }

    void print_timing_sequence()
    {
        printf("\n\n------------------------------------------\n");
        printf("        Program's name: %s\n", name.c_str());

        time_t time_sum = start_time + offset;
        char buf[30] = {0};

        for (size_t i = 0; i < steps.size(); ++i) {
            struct tm *t = localtime((time_t *)&time_sum);
            strftime(buf, 50, "%Y-%m-%d %H:%M:%S", t);
            time_sum += steps[i]->duration;
            printf("%s: reagent: %s\n", buf,
                config->reagents->operator[](steps[i]->reagent.substr(1)).operator[]("name").get<std::string>().c_str());
        }
    }

    std::string dump_end_time()
    {
        // for (size_t i = 0; i < program.steps.size(); ++i) {

        // }    
    }

    int resolveConflicts(Program *base)
    {
        // int offset = 0;
        // base.steps[]

    }

    int sumOffset(Program* program, int index)
    {
        // int sum = 0;
        // for (int i = 0; i <= index; ++i)
        //     sum += program.steps[i].duration;
        // return sum - runTime;
    }

    int find(Program* program, std::string reagent)
    {
        int index = -1;
        if (reagent == "r1")
            return index;
        for (size_t i = 0; i < program->steps.size(); ++i) {
            if (program->steps[i]->reagent == reagent) {
                index = i;
                break;
            }
        }

        return index;
    }

    std::string getName()
    {
        return name;
    }

public:
    int runTime;
    int start_time;
    int offset;
    int end_time;
    int index;
    std::string name;
    std::map<int, step *> steps;
    Configuration *config;
    int retort;
};

// class schedule_controller {
// public:
//     schedule_controller()
//     {

//     }

// };

// class device_controller {
// public:
//     device_controller()
//     {

//     }

// };

int compare_time(const Program *p1, const Program *p2)
{
    return p1->start_time < p2->start_time;
}

int main()
{
    auto config = Configuration();
    
    // load program from configuration
    auto run = config.run;
    vector<Program *> vector_;
    for (size_t i = 0; i < run->size(); ++i) {
        json &item = run->operator[](i);
        // std::cout << "run: " << item.dump() << std::endl;
        // std::cout << "program's name: " << item["program"].get<std::string>().substr(1) << std::endl;
        std::string name = item["program"].get<std::string>().substr(1);
        std::string start_time = item["start_time"].get<std::string>();
        int retort = atoi(item["retort"].get<std::string>().substr(2).c_str());
        json &j = config.programs->operator[](name);
        auto p = new Program(name, j, start_time, &config, retort);
        vector_.push_back(p);
    }

    // sort by start_time
    sort(vector_.begin(), vector_.end(), compare_time);

    // resolve conflicts
    for (size_t i = 1; i < vector_.size(); ++i) {
        Program *program = vector_[i];
        program->resolveConflicts(vector_[i - 1]);
    }

    for (size_t i = 0; i < vector_.size(); ++i) {
        vector_[i]->print_timing_sequence();
    }
#if 0
    json &cleaning = (*config.Programs)["overnight"];
    //json cleaning = Programs["cleaning"];

    std::cout << cleaning.dump().c_str() << std::endl;

    std::cout << std::endl << "cleaning size: " << cleaning.size() << std::endl;

    printf("cleaning id: %d\n", cleaning[0]["id"].get<int>());
    cleaning[0]["id"] = 10000;
    printf("cleaning id: %d\n", cleaning[0]["id"].get<int>());

    printf("cleaning reagent: %s\n", cleaning[0]["reagent"].get<std::string>().c_str());
    printf("cleaning reagent: %s\n", cleaning[0]["reagent"].get<std::string>().substr(1).c_str());

    std::string reagent_s = cleaning[0]["reagent"].get<std::string>().substr(1);

    printf("index reagent: %s\n", (*config.reagents)[reagent_s].dump().c_str());


    //printf("cleaning data : %s\n\n", cleaning.dump().c_str());
    //printf("Programs data : %s\n\n", config.Programs->dump().c_str());
#endif




    pause();

    return 0;
}
