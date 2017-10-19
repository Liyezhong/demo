#include <cstdio>
#include <fstream>
#include <iostream>
#include "json.hpp"

// for convenience
using json = nlohmann::json;
using namespace std;

class configuration {
public:
    configuration()
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

struct step {
    step(json &j)
    {
        id = j["id"].get<int>();
        reagent = j["reagent"].get<std::string>();
        duration = j["duration"].get<int>();
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
    Program(std::string _name, json &_j)
    {
        name = _name;
        index = 0;
        for (int i = 0; i < _j.size(); ++i)
            steps.insert(make_pair(i, new step(_j[i])));
    }

    void dump()
    {
        for (int i = 0; i < steps.size(); ++i)
            std::cout << "\n\n----------------------\nstep : " << steps[i]->dump();
    }

private:
    int index;
    std::string name;
    std::map<int, step *> steps;
};

class schedule_controller {
public:
    schedule_controller()
    {

    }

};

class device_controller {
public:
    device_controller()
    {

    }

};

int main()
{
    auto config = configuration();
    //std::cout << "configuration reagents: " << config.reagents->dump().c_str() << std::endl;
    //

    json &overnight = (*config.programs)["overnight"];
    Program p("overnight", overnight);
    p.dump();

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






    return 0;
}
