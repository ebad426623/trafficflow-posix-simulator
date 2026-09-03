#include "helper.h"
#include "monitor.h"
#include "WriteOutput.h"

#include<pthread.h>

#include <iostream>
#include <string>
#include <vector>
#include <queue>

using namespace std;






///////////////////// input variable declarations
int numNarrow, numFerries, numCross, numCars;
vector<int> travNarrow, travFerry, travCross;
vector<int> waitNarrow, waitFerry, waitCross;
vector<int> capacityFerry;
vector<int> travCar, pathLengthCar;
vector<vector<char>> carConnectorType;
vector<vector<int>> carConnectorID, carConnectorFrom, carConnectorTo;




////////////////////// state variable declarations
// narrow bridge variables
vector<int> narrowPassingLane; // is narrow passing lane 0 or 1
vector<vector<queue<int>>> queueNarrow; // queue of cars waiting, for each lane, for each bridge
vector<vector<int>> currentlyPassingNarrow; // how many cars passing through the bridge currently



// ferry variables
vector<vector<int>> carsOnFerry; // how many cars in a ferry at a time
vector<vector<bool>> FerryLeaving;



// crossroad variables
vector<int> crossPassingLane;
vector<vector<queue<int>>> queueCross;
vector<vector<int>> currentlyPassingCross;
vector<bool> laneChanged;



/////////////////////// helper function declarations
void takeInput();
void *CarThread(void *arg);





//////////////////////// MONITOR CLASS FOR NARROW BRIDGE FUNCTIONS
class A: public Monitor {
  vector<Condition> carConditions;
public:
A() {
}

void initialize(int n) {
  carConditions.resize(n, this);
}

int TimedWait(int i, int timeInMS) {

  struct timeval tv;
  struct timespec ts;
  
  gettimeofday(&tv, NULL);
  ts.tv_sec = tv.tv_sec + timeInMS/1000;
  ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeInMS % 1000);
  ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
  ts.tv_nsec %= (1000 * 1000 * 1000);
  
  return carConditions[i].timedwait(&ts);
  
}


void passNarrow(int idCar, int idNarrow, int from) {
  __synchronized__;
  
  queueNarrow[from][idNarrow].push(idCar);
  WriteOutput(idCar, 'N', idNarrow, ARRIVE);
  
  while (true) {
  if (narrowPassingLane[idNarrow] == from) {  // bridge direction is same as car direction
    
    if (queueNarrow[from][idNarrow].front() != idCar) { // not in front of queue
      carConditions[idCar].wait();
    }
    
    else if (currentlyPassingNarrow[!from][idNarrow] != 0) { // direction of road is in favor of curr car, but a car is coming towards it
      carConditions[idCar].wait();
    }
    
    else { // front of queue
      if (currentlyPassingNarrow[from][idNarrow] != 0) { // have to wait passdelay
        TimedWait(idCar, PASS_DELAY);
        if (narrowPassingLane[idNarrow] == from)
          goto AFTER_WAITING;
      }
      
      else {
        AFTER_WAITING:
        WriteOutput(idCar, 'N', idNarrow, START_PASSING);
        queueNarrow[from][idNarrow].pop();
        currentlyPassingNarrow[from][idNarrow]++;
        
        if (!queueNarrow[from][idNarrow].empty()) { // more cars waiting behind current
          carConditions[queueNarrow[from][idNarrow].front()].notify();
        }
        
        
        TimedWait(idCar, travNarrow[idNarrow]);
        WriteOutput(idCar, 'N', idNarrow, FINISH_PASSING);
        currentlyPassingNarrow[from][idNarrow]--;
        
        if (queueNarrow[from][idNarrow].empty() &&
        currentlyPassingNarrow[from][idNarrow] == 0 &&
        !queueNarrow[!from][idNarrow].empty()) {
        carConditions[queueNarrow[!from][idNarrow].front()].notify();
        }
        
        else if (narrowPassingLane[idNarrow] != from &&
        currentlyPassingNarrow[from][idNarrow] == 0 &&
        !queueNarrow[!from][idNarrow].empty()) { // lane changed
          carConditions[queueNarrow[!from][idNarrow].front()].notify();
        }
        return;
      }
    }
  }
  
  
  else if (queueNarrow[from][idNarrow].front() != idCar) {  // not the first in queue
    carConditions[idCar].wait();
  }
  
  else if (currentlyPassingNarrow[0][idNarrow] == 0 && currentlyPassingNarrow[1][idNarrow] == 0) {  // no cars on bridge
    narrowPassingLane[idNarrow] = from;
  }
  
  else {  // car in bridge
    TimedWait(idCar, waitNarrow[idNarrow]);
    narrowPassingLane[idNarrow] = from;
  }
  
  } // end of while loop
}

} narrow; // end of monitor class




//////////////////////// MONITOR CLASS FOR FERRY FUNCTIONS
class B: public Monitor {
  vector<vector<Condition>> FerryConditions, passing;
  Condition neverWake;
public:
B() : neverWake(this) {
}

void initialize(int n) {
  FerryConditions.resize(2);
  FerryConditions[0].resize(n, this);
  FerryConditions[1].resize(n, this);
  passing.resize(2);
  passing[0].resize(n, this);
  passing[1].resize(n, this);
}



int TimedWait(int timeInMS, Condition &c) {

  struct timeval tv;
  struct timespec ts;
  
  gettimeofday(&tv, NULL);
  ts.tv_sec = tv.tv_sec + timeInMS/1000;
  ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeInMS % 1000);
  ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
  ts.tv_nsec %= (1000 * 1000 * 1000);
  
  return c.timedwait(&ts);
  
}



void Pass(int idCar, int idFerry, int from) {
  
  carsOnFerry[from][idFerry]--;
  if (carsOnFerry[from][idFerry] == 0) {
    FerryLeaving[from][idFerry] = false;
    passing[from][idFerry].notifyAll();
  }
  
  WriteOutput(idCar, 'F', idFerry, START_PASSING);
  TimedWait(travFerry[idFerry], neverWake);
  WriteOutput(idCar, 'F', idFerry, FINISH_PASSING);
  
}

void passFerry(int idCar, int idFerry, int from) {
  __synchronized__;
  while (FerryLeaving[from][idFerry] == true)
    passing[from][idFerry].wait();
  
  WriteOutput(idCar, 'F', idFerry, ARRIVE);
  carsOnFerry[from][idFerry]++;
  
  if (carsOnFerry[from][idFerry] == capacityFerry[idFerry]) { // capacity full
    FerryConditions[from][idFerry].notifyAll();
    FerryLeaving[from][idFerry] = true;
  }
  
  else if (carsOnFerry[from][idFerry] == 1) { // capacity not full, first car
      if (TimedWait(waitFerry[idFerry], FerryConditions[from][idFerry])) {
        FerryConditions[from][idFerry].notifyAll();
        FerryLeaving[from][idFerry] = true;
        }
  }
  
  else { // capacity not full, not first car
    FerryConditions[from][idFerry].wait(); 
  }
  
  Pass(idCar, idFerry, from);
}

} ferry; // end of monitor class







//////////////////////// MONITOR CLASS FOR CROSSROAD FUNCTIONS
class C: public Monitor {
  vector<Condition> carConditions;
public:
C() {
}

void initialize(int n) {
  carConditions.resize(n, this);
}

int TimedWait(int i, int timeInMS) {

  struct timeval tv;
  struct timespec ts;
  
  gettimeofday(&tv, NULL);
  ts.tv_sec = tv.tv_sec + timeInMS/1000;
  ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeInMS % 1000);
  ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
  ts.tv_nsec %= (1000 * 1000 * 1000);
  
  return carConditions[i].timedwait(&ts);
  
}


void passCross(int idCar, int idCross, int from) {
  __synchronized__;
  
  queueCross[from][idCross].push(idCar);
  WriteOutput(idCar, 'C', idCross, ARRIVE);
  
  while (true) {
  if (crossPassingLane[idCross] == from) {  // passing direction is same as car direction
    
    if (queueCross[from][idCross].front() != idCar) { // not in front of queue
      carConditions[idCar].wait();
    }
    
    else if (currentlyPassingCross[(from + 1)%4][idCross] != 0 || 
      currentlyPassingCross[(from + 2)%4][idCross] != 0 ||
      currentlyPassingCross[(from + 3)%4][idCross] != 0) { // direction of road is in favor of curr car, but a car is coming towards it
      carConditions[idCar].wait();
    }
    
    else { // front of queue
      if (currentlyPassingCross[from][idCross] != 0) { // have to wait passdelay
        TimedWait(idCar, PASS_DELAY);
        if (crossPassingLane[idCross] == from)
          goto AFTER_WAITING;
      }
      
      else {
        AFTER_WAITING:
        WriteOutput(idCar, 'C', idCross, START_PASSING);
        queueCross[from][idCross].pop();
        currentlyPassingCross[from][idCross]++;
        
        if (!queueCross[from][idCross].empty()) { // more cars waiting behind current
          carConditions[queueCross[from][idCross].front()].notify();
        }
        
        
        TimedWait(idCar, travCross[idCross]);
        laneChanged[idCross] = false;
        WriteOutput(idCar, 'C', idCross, FINISH_PASSING);
        currentlyPassingCross[from][idCross]--;
        
        if (crossPassingLane[idCross] != from && currentlyPassingCross[from][idCross] == 0) {
          carConditions[queueCross[crossPassingLane[idCross]][idCross].front()].notify();
        }
        
        else if (queueCross[from][idCross].empty() && currentlyPassingCross[from][idCross] == 0) { // cars waiting on opposite lane
          int newLane = -1;
          for (int i = 1; i <= 3; i++) {
            if (!queueCross[(from + i)%4][idCross].empty()) { // cars waiting on opposite lane
              carConditions[queueCross[(from + i)%4][idCross].front()].notify();
              if (newLane == -1)
                newLane = (from + i)%4;
            }
          }
          if (newLane != -1) {
            crossPassingLane[idCross] = newLane;
            }
        }
        
        
        return;
      }
    }
  }
  
  
  else if (queueCross[from][idCross].size() > 1) {  // not the first in queue
    carConditions[idCar].wait();
  }
  
  else if (currentlyPassingCross[0][idCross] == 0 && 
    currentlyPassingCross[1][idCross] == 0 &&
    currentlyPassingCross[2][idCross] == 0 &&
    currentlyPassingCross[3][idCross] == 0 &&
    queueCross[(from + 1)%4][idCross].empty() &&
    queueCross[(from + 2)%4][idCross].empty() &&
    queueCross[(from + 3)%4][idCross].empty()) {  // no cars on bridge
    crossPassingLane[idCross] = from;
  }
  
  else {
  	if (TimedWait(idCar, waitCross[idCross])) { // if timed out
    
      if (laneChanged[idCross]) continue;
      int newLane = -1, currLane = crossPassingLane[idCross];
      for (int i = 1; i <= 3; i++) {
        if (!queueCross[(currLane + i)%4][idCross].empty()) { // cars waiting on opposite lane
          carConditions[queueCross[(currLane + i)%4][idCross].front()].notify();
          if (newLane == -1)
            newLane = (currLane + i)%4;
        }
      }
      if (newLane != -1) {
        crossPassingLane[idCross] = newLane;
        laneChanged[idCross] = true;
      }
    }
  }
  
  } // end of while loop
}

} cross; // end of monitor class








//////////////////////// MAIN
int main() {
  
  takeInput();
  
  InitWriteOutput();
  
  //  create threads
  pthread_t tid[numCars];
  for (int i = 0; i < numCars; i++) {
    int *arg = (int *) malloc(sizeof(*arg));
    *arg = i;
    pthread_create(&tid[i], NULL, CarThread, arg);
  }
  
  //  reap threads
  for (int i = 0; i < numCars; i++)
    pthread_join(tid[i], NULL);
  
  return 0;
  
}





// HELPER FUNCTION DEFINITIONS



void *CarThread(void *arg) {
  int id = *((int*) arg);
  for (int i = 0; i < pathLengthCar[id]; i++) {
    
    WriteOutput(id, carConnectorType[id][i], carConnectorID[id][i], TRAVEL);
    sleep_milli(travCar[id]);
    
    if (carConnectorType[id][i] == 'N')
      narrow.passNarrow(id, carConnectorID[id][i], carConnectorFrom[id][i]);
    
    else if (carConnectorType[id][i] == 'F')
      ferry.passFerry(id, carConnectorID[id][i], carConnectorFrom[id][i]);
    
    else if (carConnectorType[id][i] == 'C')
      cross.passCross(id, carConnectorID[id][i], carConnectorFrom[id][i]);
    
  }
  
  free(arg);
  return NULL;
}



void takeInput() {
  
  // narrow bridges
  cin >> numNarrow;
  queueNarrow.resize(2);
  queueNarrow[0].resize(numNarrow);
  queueNarrow[1].resize(numNarrow);
  currentlyPassingNarrow.resize(2);
  currentlyPassingNarrow[0].resize(numNarrow, 0);
  currentlyPassingNarrow[1].resize(numNarrow, 0);
  
  travNarrow.resize(numNarrow);
  waitNarrow.resize(numNarrow);
  narrowPassingLane.resize(numNarrow, 0);
  
  for (int i = 0; i < numNarrow; i++)
    cin >> travNarrow[i] >> waitNarrow[i];
  
  // ferries
  cin >> numFerries;
  
  ferry.initialize(numFerries);
  carsOnFerry.resize(2);
  carsOnFerry[0].resize(numFerries, 0);
  carsOnFerry[1].resize(numFerries, 0);
  FerryLeaving.resize(2);
  FerryLeaving[0].resize(numFerries, false);
  FerryLeaving[1].resize(numFerries, false);
  
  
  travFerry.resize(numFerries);
  waitFerry.resize(numFerries);
  capacityFerry.resize(numFerries);
  
  for (int i = 0; i < numFerries; i++)
    cin >> travFerry[i] >> waitFerry[i] >> capacityFerry[i];
  
  // crossroads
  cin >> numCross;
  crossPassingLane.resize(numCross, 0);
  laneChanged.resize(numCross, false);
  queueCross.resize(4);
  queueCross[0].resize(numCross);
  queueCross[1].resize(numCross);
  queueCross[2].resize(numCross);
  queueCross[3].resize(numCross);
  currentlyPassingCross.resize(4);
  currentlyPassingCross[0].resize(numCross, 0);
  currentlyPassingCross[1].resize(numCross, 0);
  currentlyPassingCross[2].resize(numCross, 0);
  currentlyPassingCross[3].resize(numCross, 0);
  
  travCross.resize(numCross);
  waitCross.resize(numCross);
  
  for (int i = 0; i < numCross; i++)
    cin >> travCross[i] >> waitCross[i];
  
  // cars
  cin >> numCars;
  narrow.initialize(numCars);
  cross.initialize(numCars);
  travCar.resize(numCars);
  pathLengthCar.resize(numCars);
  carConnectorType.resize(numCars);
  carConnectorID.resize(numCars);
  carConnectorFrom.resize(numCars);
  carConnectorTo.resize(numCars);
  
  string temp;
  
  for (int i = 0; i < numCars; i++) {
    cin >> travCar[i] >> pathLengthCar[i];
    
    carConnectorType[i].resize(pathLengthCar[i]);
    carConnectorID[i].resize(pathLengthCar[i]);
    carConnectorFrom[i].resize(pathLengthCar[i]);
    carConnectorTo[i].resize(pathLengthCar[i]);
    
    for (int j = 0; j < pathLengthCar[i]; j++) {
      cin >> temp >> carConnectorFrom[i][j] >> carConnectorTo[i][j];
      carConnectorType[i][j] = temp[0];
      carConnectorID[i][j] = stoi(temp.substr(1, temp.size() - 1));
    }
  }

}


