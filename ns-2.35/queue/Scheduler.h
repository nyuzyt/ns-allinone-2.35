//
// Created by Zhou Yitao on 2018-12-04.
//

#ifndef HIERARCHICALSCHEDULING_SCHEDULER_H
#define HIERARCHICALSCHEDULING_SCHEDULER_H

#include "Level.h"
#include <vector>

using namespace std;

class Scheduler {
private:
    static const int DEFAULT_VOLUME = 3;
    int volume;                     // num of levels in scheduler
    int currentRound;           // current Round
    Level levels[3];
    Level hundredLevel;
    Level decadeLevel;
public:
    Scheduler();
    explicit Scheduler(int volume);
    int push(Packet* packet, int insertLevel);
    void setCurrentRound(int currentRound);
    // Packet* serveCycle();
    // vector<Packet> serveUpperLevel(int &, int);
};


#endif //HIERARCHICALSCHEDULING_SCHEDULER_H
