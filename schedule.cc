// author : Arthur Li
// time   : 2017/9/30

#include <cstdio>
#include <fstream>
#include <iostream>
#include <ctime>
#include "json.hpp"
#include <unistd.h>
#include <memory>
#include <typeinfo>

namespace PROGRAM {

    // for convenience
    using json = nlohmann::json;
    using namespace std;

    static int __debug__ = 1;
    static int __passingPoint__ = 0;

    class Program;
    class Step;
    struct Reagent;
    struct Device;
    class ReagentManager;

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

    public:
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

        ReagentManager *reagentManager;


    private:
        json _config;
    };

    template <typename T> class TimeSlice {
        friend struct Reagent;
        friend struct Device;
        friend struct Step;
        friend class Program;
        friend void resolveConflicts(std::vector<Program *> &v);
    public:
        TimeSlice(T *parent)
            :duration(0), offset(0), extensionTime(0), lock(false), exclusive(0), parent(parent)
        {

        }

    public:
        int getStartTime();
        
        int getEndTime();

        int getSliceTime()
        {
            return duration + offset;
        }
        
        bool isOverExtensionTime()
        {
            return offset > extensionTime;
        }

        bool isLock()
        {
            return lock;
        }

        void setLock(bool l)
        {
            lock = l;
        }

        bool isExclusive()
        {
            return exclusive;
        }

        void setExclusive(bool e)
        {
            exclusive = e;
        }

        virtual void run();

    private:
        int duration;
        int offset;
        int extensionTime;
        bool lock;
        bool exclusive;

    public:
        T *parent;
        Program *program;
        Step *step;
    };
    
    struct Reagent {
        std::shared_ptr<TimeSlice<Reagent> > newTimeSlice()
        {
            std::shared_ptr<TimeSlice<Reagent> > timeSlice
                = std::make_shared<TimeSlice<Reagent> >(this);
            timeSlices.push_back(timeSlice);
            return timeSlice;
        }

        void freeTimeSlice(std::shared_ptr<TimeSlice<Reagent> >& t)
        {
            for (auto v = timeSlices.begin(); v != timeSlices.end(); ++v) {
                if ((*v) == t) {
                    timeSlices.erase(v);
                    break;
                }
            }
        }

        inline auto getTimeSliceCount()
        {
            return timeSlices.size();
        }

        std::vector<std::shared_ptr<TimeSlice<Reagent> > > timeSlices;
        std::string station;
        std::string name;
        std::string key;
    };

    struct Device {
        std::shared_ptr<TimeSlice<Device> > newTimeSlice()
        {
            std::shared_ptr<TimeSlice<Device> > timeSlice
                = std::make_shared<TimeSlice<Device> >(this);
            timeSlices.push_back(timeSlice);
            return timeSlice;
        }

        void freeTimeSlice(std::shared_ptr<TimeSlice<Device> >& t)
        {
            for (auto v = timeSlices.begin(); v != timeSlices.end(); ++v) {
                if ((*v) == t) {
                    timeSlices.erase(v);
                    break;
                }
            }
        }

        std::vector<std::shared_ptr<TimeSlice<Device> > > timeSlices;
        
        std::string name;
        std::string key; 
    };

    class ReagentManager {
    public:
        ReagentManager(Configuration& config)
        {
            auto& reagentConfig = config.reagents;
            for (json::iterator it = reagentConfig->begin(); it != reagentConfig->end(); ++it) {
                if (it.key().rfind("_group") == it.key().length() - std::string("_group").length()) {
                    // pattern match string end with _group, e.g: "r1_group", "r7_0_group"
                    std::shared_ptr<std::vector<std::string> > group = std::make_shared<std::vector<std::string> >();
                    for (auto& v: it.value()) {
                        group->push_back(v.get<std::string>());
                    }
                } else {
                    // pattern match string
                    std::shared_ptr<Reagent> reagent = std::make_shared<Reagent>();
                    reagent->key = it.key();
                    reagent->name = it.value()["name"].get<std::string>();
                    reagent->station = it.value()["station"].get<std::string>();

                    map.insert(std::make_pair(it.key(), reagent));                    
                }
            }
        }

    public:
        bool RequestTimeSlice(Step *step);   

    private:
        std::map<std::string, std::shared_ptr<Reagent> > map;
        std::map<std::string, std::shared_ptr<std::vector<std::string> > > groupMap;           
    };

    struct Step {
        
        Step(Program *parent, std::string reagent_): reagent(reagent_), passingPoint(false), parent(parent)
        {
        }

        void initReagent(json &j)
        {
            reagentTimeSlice->duration = j["duration"].get<int>() * 60;
            reagentTimeSlice->extensionTime = j["extension_time"].get<int>();

            reagentTimeSlice->program = parent;
            reagentTimeSlice->step = this;
        }

        void initDevice(json &j)
        {
            temperature = j["temperature"].get<int>();
            is_pressure = (j["pressure"].get<std::string>() == "on") ? true : false;
            is_vacuum = (j["vacuum"].get<std::string>() == "on") ? true : false;
        }

        ~Step()
        {
            // free time slice
            reagentTimeSlice->parent->freeTimeSlice(reagentTimeSlice);
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

        int getStepTime()
        {
            return getReagentTime() + getReagentTime();
        }

        int getReagentTime()
        {
            return reagentTimeSlice->getSliceTime();
        }

        int getDeviceTime()
        {
            return getDeviceEnterTime() + getDeviceLeaveTime();
        }

        int getDeviceEnterTime()
        {
            int timeSum = 0;
            for (auto &i: deviceEnterTimeSlice)
                timeSum += i->getSliceTime(); 
            return timeSum;
        }

        int getDeviceLeaveTime()
        {
            int timeSum = 0;
            for (auto &i: deviceLeaveTimeSlice)
                timeSum += i->getSliceTime(); 
            return timeSum;
        }

        bool isOverExtensionTime()
        {
            // Device Enter Time Slice
            for (auto &i: deviceEnterTimeSlice)
                if (i->isOverExtensionTime() == true)
                    return true;
            // Reagent Soaking Time Slice                    
            if (reagentTimeSlice->isOverExtensionTime() == true)
                return true;
            // Device Leave Time Slice
            for (auto &i: deviceLeaveTimeSlice)
                if (i->isOverExtensionTime() == true)
                    return true;

            return false;
        }

        int getStartTime();

        int getEndTime()
        {
            return getStartTime() + getStepTime();
        }

        void getStartTimeAndEndTime(int& startTime, int& endTime)
        {
            startTime = getStartTime();
            endTime = startTime + getStepTime();     
        }

        // bool operator==(Step& rhs)
        // {
        //     return this->id == rhs.id 
        //         && this->reagent == rhs.reagent
        //             && this->getReagentTime() == rhs.getReagentTime();
        // }

        int id;
        std::string reagent;
        int duration;
        int temperature;
        bool is_pressure;
        bool is_vacuum;

        bool passingPoint;

        Program *parent;

// TODO: PV
        std::vector<std::shared_ptr<TimeSlice<Device> > > deviceEnterTimeSlice;
        std::shared_ptr<TimeSlice<Reagent> > reagentTimeSlice;        
        std::vector<std::shared_ptr<TimeSlice<Device> > > deviceLeaveTimeSlice;        

        //
        // int start_time;
        // int start_time_offset;
        // int end_time;
        // int end_time_offset;
    };

    class Program {
        template<typename> friend class TimeSlice;
        friend struct Step;
        friend int compare(Program *p1, Program *p2);
        friend void resolveConflicts(std::vector<Program *> &v);
        friend void outputReport(std::vector<Program *> &v);
    public:
        Program(std::string _name, json& _j, std::string startTime_, Configuration *config_, int retort_, int priority_)
            : endTime(0), name(_name), config(config_), retort(retort_), priority(priority_)
        {
            for (size_t i = 0; i < _j.size(); ++i) {
                auto step = new Step(this, _j[i]["reagent"].get<std::string>().substr(1)
                     );
                step->id = i;
                config->reagentManager->RequestTimeSlice(step);

// config->reagents->operator[](_j[i]["reagent"]
//                     .get<std::string>().substr(1))["passing_point"].get<bool>()
                
                step->initReagent(_j);
                step->initDevice(_j);
                steps.insert(make_pair(i, step));
            }

            // FIXME
            baseStep = steps[0];

            struct tm tm_ = {0};
            strptime(startTime_.c_str(), "%Y-%m-%d %H:%M:%S", &tm_);
            startTime = mktime(&tm_);
        }

        ~Program()
        {
            std::map<int, Step *>::iterator it;
            for (it = steps.begin(); it != steps.end();) {
                delete it->second;
                steps.erase(it++);
            }
        }
    
    public:
        void printTimingSequence()
        {
            printf("\n\n------------------------------------------\n");
            printf("     Program's name: %s\t  retort: %d\n", name.c_str(), retort);

            time_t time_sum = startTime;
            char buf[30] = {0};

            for (size_t i = 0; i < steps.size(); ++i) {
                struct tm *t = localtime(reinterpret_cast<time_t *>(&time_sum));
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
                time_sum += steps[i]->getStepTime();
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

            time_sum += steps[0]->getStepTime();
            struct tm *t = localtime(reinterpret_cast<time_t *>(&time_sum));
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

            printf("%s: reagent: %s\n", buf,
                config->reagents->operator[](steps[0]->reagent).operator[]("name").get<std::string>().c_str());
        }

        std::string dumpEndTime()
        {
            int endTime = getEndTime();
            char buf[30] = {0};
            struct tm *t = localtime(reinterpret_cast<time_t *>(&endTime));
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);

            return std::string(buf);
        }

        void resolveConflicts(Program *base)
        {
            int offsetBackup;
            do {
                offsetBackup = this->baseStep->reagentTimeSlice->offset;
                for (size_t i = 0; i < this->steps.size(); ++i) {
                    // if (this->steps[i]->isPassingPoint() == true)
                    //     continue;
                    int j = base->find(this->steps[i]);
                    if (j == -1)
                        continue;

#if 0
                    int len;
                    if (isIntersection(base->steps[j], this->steps[i], len) == true)
                        this->baseStep->reagentTimeSlice->offset += len;
#endif

                    //////////////////////////////////////////////////////////////////
                    Step *baseStep = base->steps[j];
                    Step *selfStep = this->steps[i];

                    // Device ops Enter Time Slice
                    // TODO:                    

                    // Reagent Soaking Time Slice
                    {
                        int len;
                        if (isIntersectionTimeSlice(baseStep->reagentTimeSlice.get(),
                                selfStep->reagentTimeSlice.get(), len) == true) {
                            this->baseStep->reagentTimeSlice->offset += len;
                        }
                    }

                    // Device ops Leave Time Slice
                    // TODO:

                    //////////////////////////////////////////////////////////////////
                }
            } while (offsetBackup != this->baseStep->reagentTimeSlice->offset);
        }

        int searchPassingPointStartStep(Program *base)
        {
            for (int i = 0; i < (int)base->steps.size(); ++i) {
                int baseStepStartTime, baseStepEndTime;
                base->steps[i]->getStartTimeAndEndTime(baseStepStartTime, baseStepEndTime);
                if (this->startTime >= baseStepStartTime && this->startTime < baseStepEndTime)
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
                    this->baseStep->reagentTimeSlice->offset += len;
            }
        }

        // len:
        //      If you want to use the 'len', then s1->parent must be equal to base program.
        static bool isIntersection(Step *s1, Step *s2, int &len)
        {
            Program *base = s1->parent;
            Program *other = s2->parent;

            int baseStepEndTime = s1->getEndTime();
            int otherStepStartTime = s2->getStartTime();

            len = baseStepEndTime - otherStepStartTime;
            if ((len >= s1->getStepTime() + s2->getStepTime())
                 || len <= 0)
                return false;
            return true;
        }

        static bool isIntersectionTimeSlice(TimeSlice<Reagent> *s1, TimeSlice<Reagent> *s2, int &len)
        {
            Program *base = s1->program;
            Program *other = s2->program;

            int baseSliceEndTime = s1->getEndTime();
            int otherSliceStartTime = s2->getStartTime();

            len = baseSliceEndTime - otherSliceStartTime;
            if ((len >= s1->getSliceTime() + s2->getSliceTime())
                 || len <= 0)
                return false;
            return true;
        }

        static bool isIntersectionTimeSlice(TimeSlice<Device> *s1, TimeSlice<Device> *s2, int &len)
        {
            Program *base = s1->program;
            Program *other = s2->program;

            int baseSliceEndTime = s1->getEndTime();
            int otherSliceStartTime = s2->getStartTime();

            len = baseSliceEndTime - otherSliceStartTime;
            if ((len >= s1->getSliceTime() + s2->getSliceTime())
                 || len <= 0)
                return false;
            return true;
        }

        int find(Step *target)
        {
            int ret = -1;

            for (size_t i = 0; i < steps.size(); ++i) {
                if (steps[i]->reagentTimeSlice->parent->name == target->reagentTimeSlice->parent->name) {
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

        // result is equal to startTime + ... + --> baseStep. Excluding basestep time
        int calculationStartTime()
        {
            int sum = startTime;
            std::map<int, Step *>::iterator it = steps.begin();

            while (it->second != baseStep && it != steps.end()) {
                sum += it->second->getStepTime();
                it++;
            }

            return sum;
        }

        int getStartTime()
        {
            return startTime;
        }

        int getEndTime()
        {
            return steps[steps.size() - 1]->getEndTime();         
        }

    private:
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

    //-------------------Time Slice ------------------------------
    template <typename T>
    int TimeSlice<T>::getStartTime()
    {
        int programStartTime = this->program->getStartTime();

        if (this->parent == nullptr)
            return 0;
        if (this->program == nullptr)
            return 0;
        if (this->step == nullptr)
            return 0;

        for (int i = 0; i < this->step->id; ++i)
            programStartTime += this->program->steps[i]->getStepTime();

        // Device ops Enter time slice
        for (auto i = this->step->deviceEnterTimeSlice.begin();
                i != this->step->deviceEnterTimeSlice.end(); ++i) {
            if (typeid(*i) == typeid(this) && (size_t)i->get() == (size_t)this)
                return programStartTime;
            programStartTime += (*i)->getSliceTime();
        }
        
        // Reagent soaking time slice
        if (typeid(*this) == typeid(this->step->reagentTimeSlice))
            return programStartTime;
        else
            programStartTime += this->step->reagentTimeSlice->getSliceTime();

        // Device ops Leave time slice
        for (auto i = this->step->deviceLeaveTimeSlice.begin();
                i != this->step->deviceLeaveTimeSlice.end(); ++i) {
            if (typeid(*i) == typeid(this) && (size_t)i->get() == (size_t)this)
                return programStartTime;
            programStartTime += (*i)->getSliceTime();
        }
      
        return programStartTime;
    }

    template <typename T>
    int TimeSlice<T>::getEndTime()
    {
        return this->getStartTime() + this->getSliceTime();
    }

    //------------------- ReagentManager ------------------------
    bool ReagentManager::RequestTimeSlice(Step *step)
    {
        // process nongroup      
        auto it = groupMap.find(step->reagent);
        if (it == groupMap.end()) {
            auto it1 = map.find(step->reagent);
            if (it1 == map.end())
                return false;
            step->reagentTimeSlice = it1->second->newTimeSlice();
            return true;
        }

        // process group
        std::shared_ptr<Reagent> target = map[it->second->at(0)];
        for (size_t i = 1; i < it->second->size(); ++i) {
            auto& v = map[it->second->at(i)];
            int targetCount = target->getTimeSliceCount();
            if (targetCount == 0)
                break;
            int vCount = v->getTimeSliceCount();
            if (vCount == 0) {
                target = v;
                break;
            }
            if (target->timeSlices[targetCount - 1]->getEndTime() > v->timeSlices[vCount - 1]->getEndTime())
                target = v;          
        }

        step->reagentTimeSlice = target->newTimeSlice();

        return true;    
    }

    //------------------- Step ------------------------
    int Step::getStartTime()
    {
        if (this->parent == nullptr)
            return 0;

        int startTime = this->parent->getStartTime();

        for (int i = 0; i < this->id; ++i)
            startTime += this->parent->steps[i]->getStepTime();
        
        return startTime;            
    }

    int compare(Program *p1, Program *p2)
    {
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

    void resolveConflicts(std::vector<Program *> &v)
    {
        for (int i = 1; i < (int)v.size(); ++i) {
            int offsetBackup;
            do {
                offsetBackup = v[i]->baseStep->reagentTimeSlice->offset;
                for (int j = 0; j < i; ++j)
                    v[i]->resolveConflicts(v[j]);
            } while (offsetBackup != v[i]->baseStep->reagentTimeSlice->offset);
        }
    }

    void printTimingSequence(std::vector<Program *> &v)
    {
        if (__debug__ > 0) {
            for (auto p: v)
                p->printTimingSequence();
        }
    }

    void destory(std::vector<Program *> &v)
    {
        std::vector<Program *>::iterator it;
        for (it = v.begin(); it != v.end();) {
            delete *it;
            v.erase(it++);
        }
    }

    void outputReport(std::vector<Program *> &v)
    {
        /*
            e.g:
            "retort2" : {
            "name" : "overnight",
            "start_time": "",
            "steps" : [
                { "reagent" : "", "duration" : 10 },

            ]
        }
        */
        json report;
        for (size_t i = 0; i < v.size(); ++i) {
            report[i]["retort"] = v[i]->retort;
            report[i]["name"] = v[i]->getName();
            report[i]["start_time"] = v[i]->startTime;
            for (size_t j = 0; j < v[i]->steps.size(); ++j) {
                report[i]["steps"][j]["reagent"] = v[i]->config->reagents->operator[]
                    (v[i]->steps[j]->reagent).operator[]("name").get<std::string>();
                report[i]["steps"][j]["duration"] = v[i]->steps[j]->getStepTime();
            }
        }

        std::ofstream o("report.json");
        o << std::setw(4) << report << std::endl;
        o.close();
    }
}

int main(int argc, char *argv[])
{
    int option;
    while ((option = getopt(argc, argv, "d:p")) != -1) {
        switch (option) {
        case 'd':
            // enable/disable debug
            PROGRAM::__debug__ = atoi(optarg);
            break;
        case 'p':
            // enable passing point
            PROGRAM:: __passingPoint__ = 1;
            break;
        default:
            // Unknown option
            break;
        }
    }

    auto config = PROGRAM::Configuration();
    auto reagentManager = PROGRAM::ReagentManager(config);


    PROGRAM::Program *programPassingPoint = nullptr;

    // load program from configuration
    auto run = config.run;
    std::vector<PROGRAM::Program *> v;
    for (size_t i = 0; i < run->size(); ++i) {
        PROGRAM::json &item = run->operator[](i);
        std::string name = item["program"].get<std::string>().substr(1);
        std::string startTime = item["start_time"].get<std::string>();
        int retort = atoi(item["retort"].get<std::string>().substr(2).c_str());
        int priority = item["priority"].get<int>();
        PROGRAM::json &j = config.programs->operator[](name);
        auto program = new PROGRAM::Program(name, j, startTime, &config, retort, priority);

        // if (PROGRAM::__passingPoint__ == 1) {
        //     if (priority == 99) {
        //         programPassingPoint = program;
        //         continue;
        //     }
        // }

        // v.push_back(program);
    }
#if 0
    // sort by start_time and priority
    std::sort(v.begin(), v.end(), PROGRAM::compare);

        PROGRAM::printTimingSequence(v);;
        return 0;

    PROGRAM::resolveConflicts(v);

    if (PROGRAM::__passingPoint__ == 1) {
    // 1. Reverse overtaking point positioning
    //   1) update programPassingPoint offset
    //   2) search base program passing point
        for (std::vector<PROGRAM::Program *>::reverse_iterator it = v.rbegin();
                it != v.rend(); ++it) {
            programPassingPoint->resolveConflictsWithPassingPoint(*it);
        }

    // 2. Add programPassingPoint to program container
        v.insert(v.begin(), programPassingPoint);

    // 3. Rearrange program
        PROGRAM::resolveConflicts(v);
    }

    PROGRAM::printTimingSequence(v);

    PROGRAM::outputReport(v);

    // remember to free all resources
    PROGRAM::destory(v);
#endif
    return 0;
}
