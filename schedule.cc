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

#if 1
    json &cleaning = (*config.programs)["overnight"];
    //json cleaning = programs["cleaning"];

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
    //printf("programs data : %s\n\n", config.programs->dump().c_str());
#endif




    //for (json::iterator it = config.begin(); it != config.end(); ++it) {
        ////std::cout << it.value() << "\n";
            //std::cout << it.key() << " : " << t.value() << "\n\n\n";
        ////for (json::iterator t = it.value().begin(); t != it.value().end(); ++t) {
            ////std::cout << " : " << t.value() << "\n\n\n";
            //////std::cout << t.key() << " : " << t.value() << "\n";
        ////}
    //}

    return 0;
}
