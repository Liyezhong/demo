// author : Arthur Li
// time   : 2017/9/30

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
    Step(json &j, bool passingPoint_): offset(0),passingPoint(passingPoint_)
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

    bool isPassingPoint()
    {
        return passingPoint;
    }

    int getTotalTime()
    {
        return offset + duration;
    }

    int id;
    std::string reagent;
    int duration;
    int temperature;
    bool is_pressure;
    bool is_vacuum;

    int step_time;
    int offset; // for passing point
    bool passingPoint;
};

class Program {
public:
    Program(std::string _name, json &_j, std::string startTime_, Configuration *config_, int retort_, int priority_)
        : endTime(0), name(_name), config(config_), retort(retort_), priority(priority_)
    {
        for (size_t i = 0; i < _j.size(); ++i) {
            auto s = new Step(_j[i], config->reagents->operator[](_j[i]["reagent"].get<std::string>().substr(1))["passing_point"].get<bool>());
            steps.insert(make_pair(i, s));
        }

        baseStep = steps[0];

        struct tm tm_ = {0};
        strptime(startTime_.c_str(), "%Y-%m-%d %H:%M:%S", &tm_);
        startTime = mktime(&tm_);
    }

    void printTimingSequence()
    {
        printf("\n\n------------------------------------------\n");
        printf("     Program's name: %s\t  retort: %d\n", name.c_str(), retort);

        time_t time_sum = startTime;
        char buf[30] = {0};

        for (size_t i = 0; i < steps.size(); ++i) {
            struct tm *t = localtime((time_t *)&time_sum);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
            time_sum += steps[i]->getTotalTime();
            printf("%s: reagent: %s\n", buf,
                config->reagents->operator[](steps[i]->reagent).operator[]("name").get<std::string>().c_str());
        }
    }

    void printStartTime()
    {
        printf("\n\n------------------------------------------\n");
        printf("     Program's name: %s\t  retort: %d\n", name.c_str(), retort);

        time_t time_sum = startTime;
        char buf[30] = {0};

        time_sum += steps[0]->getTotalTime();
        struct tm *t = localtime((time_t *)&time_sum);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

        printf("%s: reagent: %s\n", buf,
            config->reagents->operator[](steps[0]->reagent).operator[]("name").get<std::string>().c_str());
    }

    std::string dumpEndTime()
    {
        endTime = startTime;

        for (size_t i = 0; i < steps.size(); ++i)
            endTime += steps[i]->getTotalTime();

        char buf[30] = {0};
        struct tm *t = localtime((time_t *)&endTime);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

        return std::string(buf);
    }


    // void resolveConflicts(Program *base)
    // {
    //     // FIXME, There may be a bug here.
    //     if ((base->startTime + base->steps[0]->offset) > startTime 
    //             && abs(base->index - this->index) == 1)
    //         this->steps[0]->offset = base->startTime + base->steps[0]->offset - startTime;

    //     int offsetBackup;
    //     do {
    //         offsetBackup = this->steps[0]->offset;
    //         for (size_t i = 0; i < steps.size(); ++i) {
    //             int j = base->find(steps[i]);

    //             if (j == -1)
    //                 continue;

    //             int baseLow, baseHigh;
    //             int curLow, curHigh;

    //             base->sumSteps(j, baseLow, baseHigh);
    //             this->sumSteps(i, curLow, curHigh);

    //             int len = baseHigh - curLow;
    //             if ((len >= base->steps[j]->duration + steps[i]->duration) || len <= 0)
    //                 continue;
    //             this->steps[0]->offset += len;
    //         }
    //     } while (offsetBackup != this->steps[0]->offset);
    // }

    void resolveConflicts(Program *base)
    {
        // FIXME, There may be a bug here.
        printf("hello\n");
        if ((base->calculationBaseTime()) > calculationBaseTime() 
                && abs(base->index - this->index) == 1)
            this->baseStep->offset = base->calculationBaseTime() - calculationBaseTime();

        int offsetBackup;
        do {
            offsetBackup = this->baseStep->offset;
            for (size_t i = 0; i < steps.size(); ++i) {
                int j = base->find(steps[i]);

                if (j == -1)
                    continue;

                int baseLow, baseHigh;
                int curLow, curHigh;

                base->sumSteps(j, baseLow, baseHigh);
                this->sumSteps(i, curLow, curHigh);

                int len = baseHigh - curLow;
                if ((len >= base->steps[j]->duration + steps[i]->duration) || len <= 0)
                    continue;
                this->baseStep->offset += len;
            }
        } while (offsetBackup != this->baseStep->offset);
    }

    int searchPassingPointStartStep(Program *base)
    {
        for (size_t i = 0; i < steps.size(); ++i) {
            int baseLow, baseHigh;
            base->sumSteps(i, baseLow, baseHigh);
            if (startTime >= baseLow && startTime < baseHigh)
                return i;
        }
        return -1;
    }

    void searchPassingPoint(Program *base)
    {
        int ret = searchPassingPointStartStep(base);
        if (ret == -1)
            return;
        for (ssize_t i = ret; i < (ssize_t)base->steps.size(); ++i) {
            if (base->steps[i]->isPassingPoint() == true) {
                // here is passing point
                base->baseStep = base->steps[i];
                break;
            }
            int j = this->find(base->steps[i]);
            if (j == -1)
                continue;

            int baseLow, baseHigh;
            int curLow, curHigh;

            base->sumSteps(i, baseLow, baseHigh);
            this->sumSteps(j, curLow, curHigh);
            int len = baseHigh - curLow;
            if ((len >= base->steps[i]->duration + steps[j]->duration) || len <= 0)
                continue;
            this->baseStep->offset += len;   
        }
    }

    void sumSteps(int step, int &low, int &high)
    {
         int sum = startTime;
         for (int i = 0; i < step; ++i)
             sum += steps[i]->getTotalTime();
         low = sum;
         sum += steps[step]->getTotalTime();
         high = sum;
    }

    int find(Step *target)
    {
        int ret = -1;
        if (target->isPassingPoint() == true)
            return ret;
        for (size_t i = 0; i < steps.size(); ++i) {
            if (steps[i]->reagent == target->reagent) {
                ret = i;
                break;
            }
        }

        return ret;
    }

    std::string getName()
    {
        return name;
    }

    void setIndex(int i)
    {
        index = i;
    }

    void setBaseStep(Step *s)
    {
        baseStep = s;
    }

    int calculationBaseTime()
    {
        int sum = startTime;
        std::map<int, Step *>::iterator it = steps.begin();

        while (it->second != baseStep && it != steps.end())
            sum += it->second->getTotalTime();

        return sum;    
    }

    int calculationStartTime()
    {
        int sum = startTime;
        std::map<int, Step *>::iterator it = steps.begin();

        do {
            sum += it->second->getTotalTime();
        } while (it->second != baseStep && it != steps.end());           

        return sum;    
    }
    
public:
    int endTime;
    int runTime;
    int startTime;
    Step *baseStep;
    std::string name;
    std::map<int, Step *> steps;
    Configuration *config;
    int retort;
    int priority;
    int index;
};

int compare(Program *p1, Program *p2)
{
    // FIXME
    return (((p1->calculationStartTime()) < (p2->calculationStartTime()))
            || (((p1->calculationStartTime()) == (p2->calculationStartTime()))
             && (p1->priority > p2->priority)));
}

#define PASSING_POINT

int main()
{
    auto config = Configuration();

#ifdef PASSING_POINT     
    Program *programPassingPoint = nullptr;
#endif

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
        auto program = new Program(name, j, startTime, &config, retort, priority);
#ifdef PASSING_POINT 
        if (priority == 99) {
            programPassingPoint = program;
            continue;
        }
#endif
        v.push_back(program);
    }

    // sort by start_time and priority
    sort(v.begin(), v.end(), compare);
    // update index
    for (size_t i = 0; i < v.size(); ++i)
        v[i]->setIndex(i);

    // resolve conflicts, no passing point
    ssize_t len = (ssize_t)v.size();
 
    for (ssize_t i = 1; i < len; ++i) {
        // FIXME
#if 1
        if (v[i]->calculationStartTime() < v[i - 1]->calculationStartTime() 
            && v[i]->startTime != v[i -1]->startTime
             && v[i]->priority > v[i - 1]->priority) {
                // v[i -1]->printStartTime();
                // v[i]->printStartTime();

            // v[i-1]->baseStep->offset = 0;
            std::swap(v[i]->index, v[i - 1]->index);
            std::swap(v[i], v[i -1]);

            i = i - 2; 
            continue;
        }
#endif        
        int offsetBackup;
        do {
            offsetBackup = v[i]->baseStep->offset;
            for (ssize_t j = i - 1; j >=0; --j)
                v[i]->resolveConflicts(v[j]);
        } while (offsetBackup != v[i]->baseStep->offset);
    }

#ifdef PASSING_POINT
// 1. Reverse overtaking point positioning
//   1) update programPassingPoint offset
//   2) search base program passing point
    for (ssize_t i = v.size() - 1; i >= 0; --i) {
        programPassingPoint->searchPassingPoint(v[i]);
    }
// 2. Add programPassingPoint to program container
#if 0
    programPassingPoint->setIndex(-1); 
    v.push_back(programPassingPoint);

    sort(v.begin(), v.end(), [](Program *p1, Program *p2) {
        return (p1->index < p2->index);
    });
    // update index
    for (size_t i = 0; i < v.size(); ++i)
        v[i]->setIndex(i);
// 3. Rearrange program  
    // for (ssize_t i = 1; i < (ssize_t)v.size(); ++i) {       
        // int offsetBackup;
        // do {
        //     offsetBackup = v[i]->baseStep->offset;
            // for (ssize_t j = i - 1; j >=0; --j)
                // v[i]->resolveConflicts(v[i-1]);
        // } while (offsetBackup != v[i]->baseStep->offset);
    // } 
    
    v[1]->resolveConflicts(v[0]);
#endif
    programPassingPoint->setIndex(-1);
    vector<Program *> vv;
    vv.push_back(programPassingPoint);
    for (size_t i = 0; i < v.size(); ++i)
        vv.push_back(v[i]);
    // vv[1]->resolveConflicts(vv[0]);
    


#endif

    for (size_t i = 0; i < vv.size(); ++i)
        vv[i]->printTimingSequence();

    pause();

    return 0;
}
