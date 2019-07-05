#include <cmath>

#include "hierarchicalQueue.h"

static class hierarchicalQueueClass : public TclClass {
public:
        hierarchicalQueueClass() : TclClass("Queue/HRCC") {}
        TclObject* create(int, const char*const*) {
	         return (new hierarchicalQueue);
	}
} class_hierarchical_queue;

hierarchicalQueue::hierarchicalQueue():hierarchicalQueue(DEFAULT_VOLUME) {
}

hierarchicalQueue::hierarchicalQueue(int volume) {
    this->volume = volume;
    flows.push_back(Flow(1, 0.2));
    // Flow(1, 0.2), Flow(2, 0.3)};
    currentRound = 0;
}

void hierarchicalQueue::setCurrentRound(int currentRound) {
    this->currentRound = currentRound;
}

void hierarchicalQueue::enque(Packet* packet) {

    hdr_ip* iph = hdr_ip::access(packet);
    int pkt_size = packet->hdrlen_ + packet->datalen();

    ///////////////////////////////////////////////////
    // TODO: get theory departure Round
    // You can get flowId from iph, then get
    // "lastDepartureRound" -- departure round of last packet of this flow
    int departureRound = cal_theory_departure_round(iph, pkt_size);
    ///////////////////////////////////////////////////

    // 20190626 Yitao
    /* With departureRound and currentRound, we can get the insertLevel, insertLevel is a parameter of flow and we can set and read this variable.
    */

    int flowId = iph->flowid();
    int insertLevel = flows[flowId].getInsertLevel();

    departureRound = max(departureRound, currentRound);
    if (departureRound / 100 != currentRound / 100 || insertLevel == 2) {
        if (departureRound / 100 % 10 == 5) {
            flows[flowId].setInsertLevel(1);
            hundredLevel.enque(packet, departureRound / 10 % 10);
        } else {
            flows[flowId].setInsertLevel(2);
            levels[2].enque(packet, departureRound / 100 % 10);
        }
    } else if (departureRound / 10 != currentRound / 10 || insertLevel == 1) {
        if (departureRound / 10 % 10 == 5) {
            flows[flowId].setInsertLevel(0);
            decadeLevel.enque(packet, departureRound  % 10);
        } else {
            flows[flowId].setInsertLevel(1);
            levels[1].enque(packet, departureRound / 10 % 10);
        }
    } else {
        flows[flowId].setInsertLevel(0);
        levels[0].enque(packet, departureRound % 10);
    }
}

// Peixuan: This can be replaced by any other algorithms
int hierarchicalQueue::cal_theory_departure_round(hdr_ip* iph, int pkt_size) {
    //int		fid_;	/* flow id */
    //int		prio_;
    // parameters in iph
    // TODO

    // Peixuan 06242019
    // For simplicity, we assume flow id = the index of array 'flows'
    int curFlowID = iph->flowid();
    float curWeight = flows[curFlowID].getWeight();
    int curLastDepartureRound = flows[curFlowID].getLastDepartureRound();
    int curStartRound = max(currentRound, curLastDepartureRound);
    int curDeaprtureRound = (int)(curStartRound + pkt_size/curWeight); // TODO: This line needs to take another thought

    // TODO: need packet length and bandwidh relation
    flows[curFlowID].setLastDepartureRound(curDeaprtureRound);
    return curDeaprtureRound;
}

//06262019 Peixuan deque function of Gearbox:

//06262019 Static getting all the departure packet in this virtual clock unit (JUST FOR SIMULATION PURPUSE!)

Packet* hierarchicalQueue::deque() {
    if (pktCurRound.size()) {
        // Pop out the first packet in pktCurRound until it is empty
        //Packet* pkt = pktCurRound.
        Packet *p = pktCurRound.front();
        pktCurRound.erase(pktCurRound.begin());
        return p;
    } else {
        pktCurRound = this->runRound();
        this->setCurrentRound(currentRound + 1); // Update system virtual clock
        this->deque();
    }

}

// Peixuan: now we only call this function to get the departure packet in the next round
vector<Packet*> hierarchicalQueue::runRound() {
    vector<Packet*> result;

    Packet* p = levels[0].deque();
    while (p) {
        result.push_back(p);
        p = levels[0].deque();
    }

    levels[0].getAndIncrementIndex();

    if (levels[0].getCurrentIndex() == 0) {
        levels[1].getAndIncrementIndex();

        if (levels[1].getCurrentIndex() == 0)
            levels[2].getAndIncrementIndex();
    }

    //current round done

    vector<Packet*> upperLevelPackets = serveUpperLevel(currentRound);

    // Peixuan
    /*for (int i = 0; i < upperLevelPackets.size(); i++) {
        packetNumRecord.push_back(packetNum);
        packetNum--;
    }*/

    result.insert(result.end(), upperLevelPackets.begin(), upperLevelPackets.end());

    //Packet p = runCycle(); Peixuan
    // backup cycle for the idling situation
    //int currentCycle_backup = currentCycle; // Peixuan: we don't need cycle now
    // if no packet in current fifo, it will return a
    // empty packet marked with packet order -1
    // Peixuan

    /*while (p.getPacketOrder() != -1) {
        packetNumRecord.push_back(packetNum);
        packetNum--;
        currentCycle++;
        p.setDepartureCycle(currentCycle);
        p.setActlDepartureRound(currentRound);
        result.push_back(p);
        p = runCycle();
    }*/

    //current round done
    //Peixuan
    //currentRound++; // Leave this to deque fucntion
    // in case there's no packet being served, cycle increase 1 as idling
    // Peixuan: we don't need cycle now
    /*if (currentCycle == currentCycle_backup) {
        packetNumRecord.push_back(packetNum);
        currentCycle++;
    }
    scheduler.setCurrentRound(currentRound);*/
    return result;
}

//Peixuan: This is also used to get the packet served in this round (VC unit)
// We need to adjust the order of serving: level0 -> level1 -> level2
vector<Packet*> hierarchicalQueue::serveUpperLevel(int currentRound) {
    vector<Packet*> result;

    // ToDo: swap the order of serving levels

    // first level 1
    if (currentRound / 10 % 10 == 5) {
        int size = decadeLevel.getCurrentFifoSize();
        for (int i = 0; i < size; i++) {
            Packet* p = decadeLevel.deque();
            if (p == 0)
                break;
            result.push_back(p);
        }
        decadeLevel.getAndIncrementIndex();
    }
    else if (!levels[1].isCurrentFifoEmpty()) {
        int size = static_cast<int>(ceil(levels[1].getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));
        for (int i = 0; i < size; i++) {
            Packet* p = levels[1].deque();
            if (p == 0)
                break;
            result.push_back(p);
        }
    }

    //then level 2
    if (currentRound / 100 % 10 == 5) {
        int size = static_cast<int>(ceil(hundredLevel.getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));
        for (int i = 0; i < size; i++) {
            Packet* p = hundredLevel.deque();
            if (p == 0)
                break;
            result.push_back(p);
        }
        if (currentRound % 10 == 9)
            hundredLevel.getAndIncrementIndex();
    }
    else if (!levels[2].isCurrentFifoEmpty()) {
        int size = static_cast<int>(ceil(levels[2].getCurrentFifoSize() * 1.0 / (100 - currentRound % 100)));
        for (int i = 0; i < size; i++) {
            Packet* p = levels[2].deque();
            if (p == 0)
                break;
            result.push_back(p);
        }
    }

    return result;
}


// This is the trail to implement the real logic
/*Packet* hierarchicalQueue::deque(){
    // If level 0 not empty, dequeue from level 0 and update the virtual clock by packet's finish time

    Packet* pkt = levels[0]->deque();
    if (pkt) {
        //this->setCurrentRound(max(pkt->departureRound, currentRound)); // update virtual clock (We don't have packet's deaprture time)
        return pkt;
    }
    pkt = levels[1]->deque();
    if (pkt) {
        return pkt;
    }
    pkt = levels[2]->deque();
    if (pkt) {
        return pkt;
    }
    // ToDo, update round and get packet from the next FIFO in level 0
    return 0;
}*/

// vector<Packet> Scheduler::serveUpperLevel(int &currentCycle, int currentRound) {
//     vector<Packet> result;

//     //first level 2
//     if (currentRound / 100 % 10 == 5) {
//         int size = static_cast<int>(ceil(hundredLevel.getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));
//         for (int i = 0; i < size; i++) {
//             Packet p = hundredLevel.pull();
//             if (p.getPacketOrder() == -1)
//                 break;
//             currentCycle++;
//             p.setDepartureCycle(currentCycle);
//             p.setActlDepartureRound(currentRound);
//             result.push_back(p);
//         }
//         if (currentRound % 10 == 9)
//             hundredLevel.getAndIncrementIndex();
//     }
//     else if (!levels[2].isCurrentFifoEmpty()) {
//         int size = static_cast<int>(ceil(levels[2].getCurrentFifoSize() * 1.0 / (100 - currentRound % 100)));
//         for (int i = 0; i < size; i++) {
//             Packet p = levels[2].pull();
//             if (p.getPacketOrder() == -1)
//                 break;
//             currentCycle++;
//             p.setDepartureCycle(currentCycle);
//             p.setActlDepartureRound(currentRound);
//             result.push_back(p);
//         }
//     }

//     // then level 1
//     if (currentRound / 10 % 10 == 5) {
//         int size = decadeLevel.getCurrentFifoSize();
//         for (int i = 0; i < size; i++) {
//             Packet p = decadeLevel.pull();
//             if (p.getPacketOrder() == -1)
//                 break;
//             currentCycle++;
//             p.setDepartureCycle(currentCycle);
//             p.setActlDepartureRound(currentRound);
//             result.push_back(p);
//         }
//         decadeLevel.getAndIncrementIndex();
//     }
//     else if (!levels[1].isCurrentFifoEmpty()) {
//         int size = static_cast<int>(ceil(levels[1].getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));
//         for (int i = 0; i < size; i++) {
//             Packet p = levels[1].pull();
//             if (p.getPacketOrder() == -1)
//                 break;
//             currentCycle++;
//             p.setDepartureCycle(currentCycle);
//             p.setActlDepartureRound(currentRound);
//             result.push_back(p);
//         }
//     }

//     return result;
// }