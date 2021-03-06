#include <cmath>

#include "AFQ.h"

static class AFQClass : public TclClass {
public:
        AFQClass() : TclClass("Queue/AFQ") {}
        TclObject* create(int, const char*const*) {
            fprintf(stderr, "Created new TCL AFQ instance\n"); // Debug: Peixuan 07062019
	        return (new AFQ);
	}
} class_hierarchical_queue;

AFQ::AFQ():AFQ(DEFAULT_VOLUME) {
    fprintf(stderr, "Created new AFQ instance\n"); // Debug: Peixuan 07062019
}

AFQ::AFQ(int volume) {
    fprintf(stderr, "Created new AFQ instance with volumn = %d\n", volume); // Debug: Peixuan 07062019
    this->volume = volume;
    flows.push_back(Flow(0, 2, 100));
    flows.push_back(Flow(1, 2, 100));
    flows.push_back(Flow(2, 2, 100));
    flows.push_back(Flow(3, 2, 100));
    flows.push_back(Flow(4, 2));        //07062019: Peixuan adding more flows for strange flow 3 problem
    flows.push_back(Flow(5, 2));        //07062019: Peixuan adding more flows for strange flow 3 problem
    flows.push_back(Flow(6, 2));        //07062019: Peixuan adding more flows for strange flow 3 problem
    //flows.push_back(Flow(1, 0.2));
    // Flow(1, 0.2), Flow(2, 0.3)};
    currentRound = 0;
    pktCount = 0; // 07072019 Peixuan
    //pktCurRound = new vector<Packet*>;
}

void AFQ::setCurrentRound(int currentRound) {
    fprintf(stderr, "Set Current Round: %d\n", currentRound); // Debug: Peixuan 07062019
    this->currentRound = currentRound;
}

void AFQ::setPktCount(int pktCount) {
    fprintf(stderr, "Set Packet Count: %d\n", pktCount); // Debug: Peixuan 07072019
    this->pktCount = pktCount;
}

void AFQ::enque(Packet* packet) {   
    
    hdr_ip* iph = hdr_ip::access(packet);
    int pkt_size = packet->hdrlen_ + packet->datalen();

    fprintf(stderr, "AAAAA Start Enqueue Flow %d Packet\n", iph->saddr()); // Debug: Peixuan 07062019

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
    //int insertLevel = flows[flowId].getInsertLevel(); //HCS->AFQ
    int insertLevel = 0;

    departureRound = max(departureRound, currentRound);

    if ((departureRound - currentRound) >= 10) {
        fprintf(stderr, "?????Exceeds maximum round, drop the packet from Flow %d\n", iph->saddr()); // Debug: Peixuan 07072019
        drop(packet);
        return;   // 07072019 Peixuan: exceeds the maximum round
    }
    
    /* if ((departureRound / 100 - currentRound / 100) >= 10) {
        fprintf(stderr, "?????Exceeds maximum round, drop the packet from Flow %d\n", iph->saddr()); // Debug: Peixuan 07072019
        drop(packet);
        return;   // 07072019 Peixuan: exceeds the maximum round
    }*/ //07252019 HCS->AFQ

    
    int curFlowID = iph->saddr();   // use source IP as flow id
    int curBrustness = flows[curFlowID].getBrustness();
    if ((departureRound - currentRound) >= curBrustness) {
        fprintf(stderr, "?????Exceeds maximum brustness, drop the packet from Flow %d\n", iph->saddr()); // Debug: Peixuan 07072019
        drop(packet);
        return;   // 07102019 Peixuan: exceeds the maximum brustness
    }

    flows[curFlowID].setLastDepartureRound(departureRound);     // 07102019 Peixuan: only update last packet finish time if the packet wasn't dropped

    /* if (departureRound / 100 != currentRound / 100 || insertLevel == 2) {
        fprintf(stderr, "Enqueue Level 2\n"); // Debug: Peixuan 07072019
        if (departureRound / 100 % 10 == 5) {
            flows[flowId].setInsertLevel(1);
            hundredLevel.enque(packet, departureRound / 10 % 10);
        } else {
            flows[flowId].setInsertLevel(2);
            levels[2].enque(packet, departureRound / 100 % 10);
        }
    } else if (departureRound / 10 != currentRound / 10 || insertLevel == 1) {
        fprintf(stderr, "Enqueue Level 1\n"); // Debug: Peixuan 07072019
        if (departureRound / 10 % 10 == 5) {
            flows[flowId].setInsertLevel(0);
            decadeLevel.enque(packet, departureRound  % 10);
        } else {
            flows[flowId].setInsertLevel(1);
            levels[1].enque(packet, departureRound / 10 % 10);
        }
    } else {
        fprintf(stderr, "Enqueue Level 0\n"); // Debug: Peixuan 07072019
        flows[flowId].setInsertLevel(0);
        levels[0].enque(packet, departureRound % 10);
    } */

    if ((departureRound - currentRound) < 10) {
        fprintf(stderr, "Enqueue Level 0\n"); // Debug: Peixuan 07072019
        flows[flowId].setInsertLevel(0);
        levels[0].enque(packet, departureRound % 10);
    } else {
        fprintf(stderr, "?????Exceeds maximum brustness, drop the packet from Flow %d\n", iph->saddr()); // Debug: Peixuan 07072019
        drop(packet);
        return;   // 07102019 Peixuan: exceeds the maximum brustness
    }



    setPktCount(pktCount + 1);
    fprintf(stderr, "Packet Count ++\n");

    fprintf(stderr, "Finish Enqueue\n"); // Debug: Peixuan 07062019
}

// Peixuan: This can be replaced by any other algorithms
int AFQ::cal_theory_departure_round(hdr_ip* iph, int pkt_size) {
    //int		fid_;	/* flow id */
    //int		prio_;
    // parameters in iph
    // TODO

    // Peixuan 06242019
    // For simplicity, we assume flow id = the index of array 'flows'

    fprintf(stderr, "$$$$$Calculate Departure Round at VC = %d\n", currentRound); // Debug: Peixuan 07062019

    int curFlowID = iph->saddr();   // use source IP as flow id
    float curWeight = flows[curFlowID].getWeight();
    int curLastDepartureRound = flows[curFlowID].getLastDepartureRound();
    int curStartRound = max(currentRound, curLastDepartureRound);

    fprintf(stderr, "$$$$$Last Departure Round of Flow%d = %d\n",iph->saddr() , curLastDepartureRound); // Debug: Peixuan 07062019
    fprintf(stderr, "$$$$$Start Departure Round of Flow%d = %d\n",iph->saddr() , curStartRound); // Debug: Peixuan 07062019

    //int curDeaprtureRound = (int)(curStartRound + pkt_size/curWeight); // TODO: This line needs to take another thought

    int curDeaprtureRound = (int)(curStartRound + curWeight); // 07072019 Peixuan: basic test

    fprintf(stderr, "$$$$$Calculated Packet From Flow:%d with Departure Round = %d\n",iph->saddr() , curDeaprtureRound); // Debug: Peixuan 07062019
    // TODO: need packet length and bandwidh relation
    //flows[curFlowID].setLastDepartureRound(curDeaprtureRound);
    return curDeaprtureRound;
}

//06262019 Peixuan deque function of Gearbox:

//06262019 Static getting all the departure packet in this virtual clock unit (JUST FOR SIMULATION PURPUSE!)

Packet* AFQ::deque() {

    fprintf(stderr, "Start Dequeue\n"); // Debug: Peixuan 07062019

    //fprintf(stderr, "Queue size: %d\n",pktCurRound.size()); // Debug: Peixuan 07062019

    if (pktCount == 0) {
        fprintf(stderr, "Scheduler Empty\n"); // Debug: Peixuan 07062019
        return 0;
    }

    while (!pktCurRound.size()) {
        fprintf(stderr, "Empty Round\n"); // Debug: Peixuan 07062019
        pktCurRound = this->runRound();
        this->setCurrentRound(currentRound + 1); // Update system virtual clock
        //this->deque();
    }

    Packet *p = pktCurRound.front();
    pktCurRound.erase(pktCurRound.begin());

    setPktCount(pktCount - 1);
    fprintf(stderr, "Packet Count --\n");

    hdr_ip* iph = hdr_ip::access(p);
    fprintf(stderr, "*****Dequeue Packet p with soure IP: %x\n", iph->saddr()); // Debug: Peixuan 07062019

    // Printing sequence test
    // hdr_tcp* tcph = hdr_tcp::access(p);

    return p;



    /*if (pktCurRound.size()) {
        // Pop out the first packet in pktCurRound until it is empty
        //Packet* pkt = pktCurRound.
        Packet *p = pktCurRound.front();
        pktCurRound.erase(pktCurRound.begin());

        hdr_ip* iph = hdr_ip::access(p);
        fprintf(stderr, "Dequeue Packet p with soure IP: %x\n", iph->saddr()); // Debug: Peixuan 07062019
        return p;
    } else {
        fprintf(stderr, "Empty Round\n"); // Debug: Peixuan 07062019
        pktCurRound = this->runRound();
        this->setCurrentRound(currentRound + 1); // Update system virtual clock
        this->deque();
    }*/

}

// Peixuan: now we only call this function to get the departure packet in the next round
vector<Packet*> AFQ::runRound() {

    fprintf(stderr, "Run Round\n"); // Debug: Peixuan 07062019

    vector<Packet*> result;

    // Debug: Peixuan 07062019: Bug Founded: What if the queue is empty at the moment? Check Size!

    //current round done

    //vector<Packet*> upperLevelPackets = serveUpperLevel(currentRound);    // HCS -> AFQ

    // Peixuan
    /*for (int i = 0; i < upperLevelPackets.size(); i++) {
        packetNumRecord.push_back(packetNum);
        packetNum--;
    }*/

    //result.insert(result.end(), upperLevelPackets.begin(), upperLevelPackets.end());  // HCS -> AFQ
    
    //fprintf(stderr, "Extracting packet\n"); // Debug: Peixuan 07062019

    Packet* p = levels[0].deque();

    //fprintf(stderr, "Get packet pointer\n"); // Debug: Peixuan 07062019

    if (!p) {
        fprintf(stderr, "No packet\n"); // Debug: Peixuan 07062019
    }

    while (p) {

        hdr_ip* iph = hdr_ip::access(p);                   // 07092019 Peixuan Debug

        fprintf(stderr, "^^^^^At Round:%d, Round Deque Flow %d Packet From Level 0: fifo %d\n", currentRound, iph->saddr(), levels[0].getCurrentIndex()); // Debug: Peixuan 07092019

        result.push_back(p);
        p = levels[0].deque();
    }

    levels[0].getAndIncrementIndex();               // Level 0 move to next FIFO
    fprintf(stderr, "<<<<<At Round:%d, Level 0 update current FIFO as: fifo %d\n", currentRound, levels[0].getCurrentIndex()); // Debug: Peixuan 07212019

    /* if (levels[0].getCurrentIndex() == 0) {
        levels[1].getAndIncrementIndex();           // Level 1 move to next FIFO
        fprintf(stderr, "<<<<<At Round:%d, Level 1 update current FIFO as: fifo %d\n", currentRound, levels[1].getCurrentIndex()); // Debug: Peixuan 07212019

        if (levels[1].getCurrentIndex() == 0)
            levels[2].getAndIncrementIndex();       // Level 2 move to next FIFO
            fprintf(stderr, "<<<<<At Round:%d, Level 2 update current FIFO as: fifo %d\n", currentRound, levels[2].getCurrentIndex()); // Debug: Peixuan 07212019
    }*/ // HCS -> AFQ

    // 07212019 Change Serving Order
    
    //current round done

    //0721 vector<Packet*> upperLevelPackets = serveUpperLevel(currentRound);

    // Peixuan
    /*for (int i = 0; i < upperLevelPackets.size(); i++) {
        packetNumRecord.push_back(packetNum);
        packetNum--;
    }*/

    //0721 result.insert(result.end(), upperLevelPackets.begin(), upperLevelPackets.end());

    // 07212019 Change Serving Order

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
/* vector<Packet*> AFQ::serveUpperLevel(int currentRound) {

    fprintf(stderr, "Serving Upper Level\n"); // Debug: Peixuan 07062019

    vector<Packet*> result;

    // ToDo: swap the order of serving levels

    //First: then level 2
    if (currentRound / 100 % 10 == 5) {
        //int size = static_cast<int>(ceil(hundredLevel.getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));  // 07212019 Peixuan *** Fix Level 2 serving order (ori)
        int size = static_cast<int>(ceil((hundredLevel.getCurrentFifoSize() + levels[1].getCurrentFifoSize()) * 1.0 / (10 - currentRound % 10)));  // 07212019 Peixuan *** Fix Level 2 serving order (fixed)
        //int size = hundredLevel.getCurrentFifoSize();
        for (int i = 0; i < size; i++) {
            Packet* p = hundredLevel.deque();
            if (p == 0)
                break;
            hdr_ip* iph = hdr_ip::access(p);                   // 07092019 Peixuan Debug

            fprintf(stderr, "^^^^^At Round:%d, Round Deque Flow %d Packet From Level 2 Convergence FIFO: fifo %d\n", currentRound, iph->saddr(), hundredLevel.getCurrentIndex()); // Debug: Peixuan 07092019
            result.push_back(p);
        }

        if (hundredLevel.getCurrentFifoSize() && currentRound / 10 % 10 != 5)  // 07222019 Peixuan ***: If hundredLevel not empty, serve it until it is empty (Except Level 1 is serving Convergence FIFO (decade FIFO))
        //if (hundredLevel.getCurrentFifoSize())  // 07212019 Peixuan ***: If hundredLevel not empty, serve it until it is empty
            return result;                      // 07212019 Peixuan ***

        if (currentRound % 10 == 9)
            hundredLevel.getAndIncrementIndex();

    }
    else if (!levels[2].isCurrentFifoEmpty()) {
        int size = static_cast<int>(ceil(levels[2].getCurrentFifoSize() * 1.0 / (100 - currentRound % 100)));
        for (int i = 0; i < size; i++) {
            Packet* p = levels[2].deque();
            if (p == 0)
                break;
            hdr_ip* iph = hdr_ip::access(p);                   // 07092019 Peixuan Debug

            fprintf(stderr, "^^^^^At Round:%d, Round Deque Flow %d Packet From Level 2, fifo: %d\n", currentRound, iph->saddr(), levels[2].getCurrentIndex()); // Debug: Peixuan 07092019
            result.push_back(p);
        }
    }

    //Then: first level 1
    if (currentRound / 10 % 10 == 5) {
        int size = decadeLevel.getCurrentFifoSize();
        fprintf(stderr, ">>>>>At Round:%d, Serve Level 1 Convergence FIFO with fifo: %d, size: %d\n", currentRound, decadeLevel.getCurrentIndex(), size); // Debug: Peixuan 07222019
        for (int i = 0; i < size; i++) {
            Packet* p = decadeLevel.deque();
            if (p == 0)
                break;
            result.push_back(p);

            hdr_ip* iph = hdr_ip::access(p);                   // 07092019 Peixuan Debug

            fprintf(stderr, "^^^^^At Round:%d, Round Deque Flow %d Packet From Level 1 Convergence FIFO, fifo: %d\n", currentRound, iph->saddr(), decadeLevel.getCurrentIndex()); // Debug: Peixuan 07092019

        }
        decadeLevel.getAndIncrementIndex();
        
    }
    else if (!levels[1].isCurrentFifoEmpty()) {
        int size = static_cast<int>(ceil(levels[1].getCurrentFifoSize() * 1.0 / (10 - currentRound % 10)));   // 07212019 Peixuan *** Fix Level 1 serving order (ori)
        //int size = static_cast<int>(ceil((hundredLevel.getCurrentFifoSize() + levels[1].getCurrentFifoSize()) * 1.0 / (10 - currentRound % 10)));  // 07212019 Peixuan *** Fix Level 1 serving order (fixed)
        fprintf(stderr, ">>>At Round:%d, Serve Level 1 Regular FIFO with fifo: %d, size: %d\n", currentRound, levels[1].getCurrentIndex(), size); // Debug: Peixuan 07222019
        for (int i = 0; i < size; i++) {
            Packet* p = levels[1].deque();
            if (p == 0)
                break;
            hdr_ip* iph = hdr_ip::access(p);                   // 07092019 Peixuan Debug

            fprintf(stderr, "^^^^^At Round:%d, Round Deque Flow %d Packet From Level 1, fifo: %d\n", currentRound, iph->saddr(), levels[1].getCurrentIndex()); // Debug: Peixuan 07092019
            result.push_back(p);
        }
    }

   

    return result;
}*/   // HCS -> AFQ


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