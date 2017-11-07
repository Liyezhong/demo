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

class Program;

struct Step {
    Step(json &j, bool passingPoint_): offset(0), passingPoint(passingPoint_)
    {
        // id = j["id"].get<int>();
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

    Program *parent;
};

class Program {
public:
    Program(std::string _name, json &_j, std::string startTime_, Configuration *config_, int retort_, int priority_)
        : endTime(0), name(_name), config(config_), retort(retort_), priority(priority_)
    {
        for (size_t i = 0; i < _j.size(); ++i) {
            auto s = new Step(_j[i], config->reagents->operator[](_j[i]["reagent"].get<std::string>().substr(1))["passing_point"].get<bool>());
            s->id = i;
            s->parent = this;
            steps.insert(make_pair(i, s));
        }

        // FIXME
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
        int endTime = startTime;

        for (size_t i = 0; i < steps.size(); ++i)
            endTime += steps[i]->getTotalTime();

        char buf[30] = {0};
        struct tm *t = localtime((time_t *)&endTime);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

        return std::string(buf);
    }

    void resolveConflicts(Program *base)
    {
        int offsetBackup;
        do {
            offsetBackup = this->baseStep->offset;
            for (size_t i = 0; i < this->steps.size(); ++i) {
                if (this->steps[i]->isPassingPoint() == true)
                    continue;
                int j = base->find(this->steps[i]);
                if (j == -1)
                    continue;

                int len;
                if (isIntersection(base->steps[j], this->steps[i], len) == true)
                    this->baseStep->offset += len;
            }
        } while (offsetBackup != this->baseStep->offset);
    }

    int searchPassingPointStartStep(Program *base)
    {
        for (int i = 0; i < (int)base->steps.size(); ++i) {
            int baseLow, baseHigh;
            base->sumSteps(base->steps[i], baseLow, baseHigh);
            if (this->startTime >= baseLow && this->startTime < baseHigh)
                return i;
        }
        return -1;
    }

    void resolveConflictsWithPassingPoint(Program *base)
    {
        int ret = this->searchPassingPointStartStep(base);
        if (ret == -1)
            return;
        for (int i = ret; i < (int)base->steps.size(); ++i) {
            if (base->steps[i]->isPassingPoint() == true) {
                // here is passing point
                base->baseStep = base->steps[i];
                break;
            }
            int j = this->find(base->steps[i]);
            if (j == -1)
                continue;
            int len;
            if (isIntersection(base->steps[i], this->steps[j], len) == true)
                this->baseStep->offset += len;
        }
    }

    // 如果要用len, 那么s1->parent必须是base program, 否则len会有问题
    static bool isIntersection(Step *s1, Step *s2, int &len)
    {
        int baseLow, baseHigh;
        int otherLow, otherHigh;

        Program *base = s1->parent;
        Program *other = s2->parent;

        base->sumSteps(s1, baseLow, baseHigh);
        other->sumSteps(s2, otherLow, otherHigh);
        len = baseHigh - otherLow;
        if ((len >= base->steps[s1->id]->duration + other->steps[s2->id]->duration)
             || len <= 0)
            return false;
        return true;       
    }

    void sumSteps(Step *s, int &low, int &high)
    {
        int sum = startTime;

        int index;
        if (s->parent != this) {
            index = find(s);
            if (index == -1)
                return;
        } else {
            index = s->id;
        }

        for (int i = 0; i < index; ++i)
            sum += steps[i]->getTotalTime();
        low = sum;
        sum += steps[index]->getTotalTime();
        high = sum;         
    }

    int find(Step *target)
    {
        int ret = -1;

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

    void setBaseStep(Step *s)
    {
        if (s->parent != this)
            return;
        baseStep = s;
    }

    // start time 到 basestep所用的时间, 不包括basestep时间
    int calculationStartTime()
    {
        int sum = startTime;
        std::map<int, Step *>::iterator it = steps.begin();

        while (it->second != baseStep && it != steps.end()) {
            sum += it->second->getTotalTime();
            it++;
        }        

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
};

int compare(Program *p1, Program *p2)
{
    // FIXME
    int len;
    if (Program::isIntersection(p1->baseStep, p2->baseStep, len) == true) {
        if (p1->priority == p2->priority)
            return p1->calculationStartTime() < p2->calculationStartTime();
        else
            return p1->priority > p2->priority;
    } else {
        return p1->calculationStartTime() < p2->calculationStartTime();
    }
}

#define PASSING_POINT
#define DEBUG

int main()
{
    auto config = Configuration();

#ifdef PASSING_POINT     
    Program *programPassingPoint = nullptr;
#endif

    // load program from configuration
    auto run = config.run;
    std::vector<Program *> v;
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
    std::sort(v.begin(), v.end(), compare);

    for (int i = 1; i < (int)v.size(); ++i) {
        int offsetBackup;
        do {
            offsetBackup = v[i]->baseStep->offset;
            for (int j = 0; j < i; ++j)
                v[i]->resolveConflicts(v[j]);
        } while (offsetBackup != v[i]->baseStep->offset);
    }

#ifdef DEBUG
    // for (auto p: v)
    //     p->printTimingSequence();
#endif

#ifdef PASSING_POINT
// 1. Reverse overtaking point positioning
//   1) update programPassingPoint offset
//   2) search base program passing point
    for (std::vector<Program *>::reverse_iterator it = v.rbegin(); it != v.rend(); ++it) {
        programPassingPoint->resolveConflictsWithPassingPoint(*it);    
    }
// 2. Add programPassingPoint to program container
    v.insert(v.begin(), programPassingPoint);

// 3. Rearrange program
    for (int i = 1; i < (int)v.size(); ++i) {
        int offsetBackup;
        do {
            offsetBackup = v[i]->baseStep->offset;
            for (int j = 0; j < i; ++j)
                v[i]->resolveConflicts(v[j]);
        } while (offsetBackup != v[i]->baseStep->offset);
    }
#endif

#ifdef DEBUG
    for (auto p: v)
        p->printTimingSequence();
#endif

    // remember to free all resources
    
    pause();

    return 0;
}
