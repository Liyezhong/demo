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

struct Step {
    Step(json &j)
    {
        id = j["id"].get<int>();
        reagent = j["reagent"].get<std::string>().substr(1);
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
    Program(std::string _name, json &_j, std::string startTime_, Configuration *config_, int retort_, int priority_)
        : endTime(0), offset(0), name(_name), config(config_), retort(retort_), priority(priority_)
    {
        for (size_t i = 0; i < _j.size(); ++i)
            steps.insert(make_pair(i, new Step(_j[i])));
        struct tm tm_ = {0};
        strptime(startTime_.c_str(), "%Y-%m-%d %H:%M:%S", &tm_);
        startTime = mktime(&tm_);
    }

    void printTimingSequence()
    {
        printf("\n\n------------------------------------------\n");
        printf("     Program's name: %s\t  retort: %d\n", name.c_str(), retort);

        time_t time_sum = startTime + offset;
        char buf[30] = {0};

        for (size_t i = 0; i < steps.size(); ++i) {
            struct tm *t = localtime((time_t *)&time_sum);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
            time_sum += steps[i]->duration;
            printf("%s: reagent: %s\n", buf,
                config->reagents->operator[](steps[i]->reagent).operator[]("name").get<std::string>().c_str());
        }
    }

    std::string dumpEndTime()
    {
        endTime = startTime + offset;

        for (size_t i = 0; i < steps.size(); ++i)
            endTime += steps[i]->duration;

        char buf[30] = {0};
        struct tm *t = localtime((time_t *)&endTime);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

        return std::string(buf);
    }

    void resolveConflicts(Program *base)
    {
        // FIXME, There may be a bug here.
        if (base->startTime + base->offset > startTime)
            offset = base->startTime + base->offset - startTime;

        int offsetBackup;
        do {
            offsetBackup = offset;
            for (size_t i = 0; i < steps.size(); ++i) {
                int j = base->find(steps[i]->reagent);
    
                if (j == -1)
                    continue;
    
                int baseLow, baseHigh;
                int curLow, curHigh;
    
                base->sumSteps(j, baseLow, baseHigh);
                this->sumSteps(i, curLow, curHigh);
    
                int len = baseHigh - curLow;
                if ((len >= base->steps[j]->duration + steps[i]->duration) || len <= 0)
                    continue;
                offset += len;
            }
        } while (offsetBackup != offset);
    }

    void sumSteps(int step, int &low, int &high)
    {
         int sum = startTime + offset;
         for (int i = 0; i < step; ++i)
             sum += steps[i]->duration;
         low = sum;
         sum += steps[step]->duration;
         high = sum;
    }

    int find(std::string reagent)
    {
        int index = -1;
        if (reagent == "r1")
            return index;
        for (size_t i = 0; i < steps.size(); ++i) {
            if (steps[i]->reagent == reagent) {
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
    int endTime;
    int runTime;
    int startTime;
    int offset;
    std::string name;
    std::map<int, Step *> steps;
    Configuration *config;
    int retort;
    int priority;
};

int compare(Program *p1, Program *p2)
{
    // The formalin phase needs special treatment. 
    return (((p1->startTime + p1->steps[0]->duration) < (p2->startTime + p2->steps[0]->duration))
            || (p1->priority > p2->priority));
}

int main()
{
    auto config = Configuration();

    // load program from configuration
    auto run = config.run;
    vector<Program *> v;
    for (size_t i = 0; i < run->size(); ++i) {
        json &item = run->operator[](i);
        std::string name = item["program"].get<std::string>().substr(1);
        std::string startTime = item["start_time"].get<std::string>();
        int retort = atoi(item["retort"].get<std::string>().substr(2).c_str());
        int priority = item["priority"].get<int>();
        json &j = config.programs->operator[](name);
        auto p = new Program(name, j, startTime, &config, retort, priority);
        v.push_back(p);
    }

    // sort by start_time and priority
    sort(v.begin(), v.end(), compare);

    // resolve conflicts
    for (ssize_t i = 1; i < (ssize_t)v.size(); ++i) {
        for (ssize_t j = i - 1; j >=0; --j) {
            v[i]->resolveConflicts(v[j]);
        }
    }

    for (size_t i = 0; i < v.size(); ++i) {
        v[i]->printTimingSequence();
    }

    pause();

    return 0;
}
