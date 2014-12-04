/**
 * EPNex.cxx
 *
 * @since 2013-01-09
 * @author D. Klein, A. Rybalchenko, M.Al-Turany, C. Kouzinopoulos
 */

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "EPNex.h"
#include "FairMQLogger.h"

using namespace std;
using boost::posix_time::ptime;

using namespace AliceO2::Devices;

EPNex::EPNex() :
  fHeartbeatIntervalInMs(3000)
{
}

EPNex::~EPNex()
{
}

void EPNex::PrintBuffer(unordered_map<int,int> &eventBuffer)
{
  int size = eventBuffer.size();
  string header = "===== ";

  for (int i = 1; i <= fNumOutputs; ++i) {
    stringstream out;
    out << i % 10;
    header += out.str();
    //i > 9 ? header += " " : header += "  ";
  }
  LOG(INFO) << header;

  for (unordered_map<int,int>::iterator it = eventBuffer.begin(); it != eventBuffer.end(); ++it) {
    string stars = "";
    for (int j = 1; j <= it->second; ++j) {
      stars += "*";
    }
    LOG(INFO) << setw(4) << it->first << ": " << stars;
  }
}

void EPNex::Run()
{
  LOG(INFO) << ">>>>>>> Run <<<<<<<";

  boost::thread rateLogger(boost::bind(&FairMQDevice::LogSocketRates, this));
  boost::thread heartbeatSender(boost::bind(&EPNex::sendHeartbeats, this));

  // Currently number of outputs equals number of FLPs, each output = heartbeat channel.
  // This may need to be changed in future.
  const int numFLPs = fNumOutputs;

  unsigned long fullEvents = 0;
  ptime time_start;
  ptime time_end;

  while (fState == RUNNING) {
    // Receive payload
    FairMQMessage* idPart = fTransportFactory->CreateMessage();

    if (fPayloadInputs->at(0)->Receive(idPart) > 0) {
      unsigned long* id = reinterpret_cast<unsigned long*>(idPart->GetData());
      // LOG(INFO) << "Received Event #" << *id;

      if (fEventBuffer.find(*id) == fEventBuffer.end()) {
        fEventBuffer[*id] = 1;
        PrintBuffer(fEventBuffer);
      } else {
        ++fEventBuffer[*id];
        PrintBuffer(fEventBuffer);
        if (fEventBuffer[*id] == numFLPs) {
          // LOG(INFO) << "collected " << numFLPs << " parts of event #" << *id << ", processing...";
          // LOG(INFO) << "# of full events: " << ++fullEvents;
          ++fullEvents;
          if (fullEvents == 1) {
            time_start = boost::posix_time::microsec_clock::local_time();
          } else if (fullEvents == 100) {
            time_end = boost::posix_time::microsec_clock::local_time();
            LOG(WARN) << "Received 100 events in " << (time_end - time_start).total_milliseconds() << " milliseconds.";
          }
          fEventBuffer.erase(*id);
          // LOG(INFO) << "size of eventBuffer: " << eventBuffer.size();
        }
      }

      FairMQMessage* dataPart = fTransportFactory->CreateMessage();

      if (fPayloadInputs->at(0)->Receive(dataPart) > 0) {
        // ... do something with data here ...
      }
      delete dataPart;
    }
    delete idPart;
  }

  rateLogger.interrupt();
  rateLogger.join();

  heartbeatSender.interrupt();
  heartbeatSender.join();

  FairMQDevice::Shutdown();

  // notify parent thread about end of processing.
  boost::lock_guard<boost::mutex> lock(fRunningMutex);
  fRunningFinished = true;
  fRunningCondition.notify_one();
}

void EPNex::sendHeartbeats()
{
  while (true) {
    try {
      for (int i = 0; i < fNumOutputs; ++i) {
        FairMQMessage* heartbeatMsg = fTransportFactory->CreateMessage(fInputAddress.at(0).size());
        memcpy(heartbeatMsg->GetData(), fInputAddress.at(0).c_str(), fInputAddress.at(0).size());

        fPayloadOutputs->at(i)->Send(heartbeatMsg);

        delete heartbeatMsg;
      }
      boost::this_thread::sleep(boost::posix_time::milliseconds(fHeartbeatIntervalInMs));
    } catch (boost::thread_interrupted&) {
      LOG(INFO) << "EPNex::sendHeartbeat() interrupted";
      break;
    }
  } // while (true)
}

void EPNex::SetProperty(const int key, const string& value, const int slot/*= 0*/)
{
  switch (key) {
    default:
      FairMQDevice::SetProperty(key, value, slot);
      break;
  }
}

string EPNex::GetProperty(const int key, const string& default_/*= ""*/, const int slot/*= 0*/)
{
  switch (key) {
    default:
      return FairMQDevice::GetProperty(key, default_, slot);
  }
}

void EPNex::SetProperty(const int key, const int value, const int slot/*= 0*/)
{
  switch (key) {
    case HeartbeatIntervalInMs:
      fHeartbeatIntervalInMs = value;
      break;
    default:
      FairMQDevice::SetProperty(key, value, slot);
      break;
  }
}

int EPNex::GetProperty(const int key, const int default_/*= 0*/, const int slot/*= 0*/)
{
  switch (key) {
    case HeartbeatIntervalInMs:
      return fHeartbeatIntervalInMs;
    default:
      return FairMQDevice::GetProperty(key, default_, slot);
  }
}
