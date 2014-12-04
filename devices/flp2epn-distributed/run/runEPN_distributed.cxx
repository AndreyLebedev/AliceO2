/**
 * runEPN_distributed.cxx
 *
 * @since 2013-01-21
 * @author D. Klein, A. Rybalchenko, M. Al-Turany, C. Kouzinopoulos
 */

#include <iostream>
#include <csignal>

#include "boost/program_options.hpp"

#include "FairMQLogger.h"
#include "EPNex.h"

#ifdef NANOMSG
#include "FairMQTransportFactoryNN.h"
#else
#include "FairMQTransportFactoryZMQ.h"
#endif

//DDS
#include "KeyValue.h"
#include <boost/asio.hpp>

using namespace std;

using namespace AliceO2::Devices;

EPNex epn;

static void s_signal_handler (int signal)
{
  cout << endl << "Caught signal " << signal << endl;

  epn.ChangeState(EPNex::STOP);
  epn.ChangeState(EPNex::END);

  cout << "Shutdown complete. Bye!" << endl;
  exit(1);
}

static void s_catch_signals (void)
{
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
}

typedef struct DeviceOptions
{
  string id;
  int ioThreads;
  int numOutputs;
  int heartbeatIntervalInMs;
  string inputSocketType;
  int inputBufSize;
  string inputMethod;
//  string inputAddress;
  int logInputRate;
  string outputSocketType;
  int outputBufSize;
  string outputMethod;
//  vector<string> outputAddress;
  int logOutputRate;
} DeviceOptions_t;

inline bool parse_cmd_line(int _argc, char* _argv[], DeviceOptions* _options)
{
  if (_options == NULL)
    throw std::runtime_error("Internal error: options' container is empty.");

  namespace bpo = boost::program_options;
  bpo::options_description desc("Options");
  desc.add_options()
    ("id", bpo::value<string>()->required(), "Device ID")
    ("io-threads", bpo::value<int>()->default_value(1), "Number of I/O threads")
    ("num-outputs", bpo::value<int>()->required(), "Number of EPN output sockets")
    ("heartbeat-interval", bpo::value<int>()->default_value(5000), "Heartbeat interval in milliseconds")
    ("input-socket-type", bpo::value<string>()->required(), "Input socket type: sub/pull")
    ("input-buff-size", bpo::value<int>()->required(), "Input buffer size in number of messages (ZeroMQ)/bytes(nanomsg)")
    ("input-method", bpo::value<string>()->required(), "Input method: bind/connect")
//    ("input-address", bpo::value<string>()->required(), "Input address, e.g.: \"tcp://localhost:5555\"")
    ("log-input-rate", bpo::value<int>()->required(), "Log input rate on socket, 1/0")
    ("output-socket-type", bpo::value<string>()->required(), "Output socket type: pub/push")
    ("output-buff-size", bpo::value<int>()->required(), "Output buffer size in number of messages (ZeroMQ)/bytes(nanomsg)")
    ("output-method", bpo::value<string>()->required(), "Output method: bind/connect")
//    ("output-address", bpo::value< vector<string> >()->required(), "Output address, e.g.: \"tcp://localhost:5555\"")
    ("log-output-rate", bpo::value<int>()->required(), "Log output rate on socket, 1/0")
    ("help", "Print help messages");

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(_argc, _argv, desc), vm);

  if (vm.count("help")) {
    LOG(INFO) << "EPN" << endl << desc;
    return false;
  }

  bpo::notify(vm);

  if (vm.count("id")) {
    _options->id = vm["id"].as<string>();
  }

  if (vm.count("io-threads")) {
    _options->ioThreads = vm["io-threads"].as<int>();
  }

  if (vm.count("num-outputs")) {
    _options->numOutputs = vm["num-outputs"].as<int>();
  }

  if (vm.count("heartbeat-interval")) {
    _options->heartbeatIntervalInMs = vm["heartbeat-interval"].as<int>();
  }

  if (vm.count("input-socket-type")) {
    _options->inputSocketType = vm["input-socket-type"].as<string>();
  }

  if (vm.count("input-buff-size")) {
    _options->inputBufSize = vm["input-buff-size"].as<int>();
  }

  if (vm.count("input-method")) {
    _options->inputMethod = vm["input-method"].as<string>();
  }

//  if (vm.count("input-address")) {
//    _options->inputAddress = vm["input-address"].as<string>();
//  }

  if (vm.count("log-input-rate")) {
    _options->logInputRate = vm["log-input-rate"].as<int>();
  }

  if (vm.count("output-socket-type")) {
    _options->outputSocketType = vm["output-socket-type"].as<string>();
  }

  if (vm.count("output-buff-size")) {
    _options->outputBufSize = vm["output-buff-size"].as<int>();
  }

  if (vm.count("output-method")) {
    _options->outputMethod = vm["output-method"].as<string>();
  }

//  if (vm.count("output-address")) {
//    _options->outputAddress = vm["output-address"].as<vector<string>>();
//  }

  if (vm.count("log-output-rate")) {
    _options->logOutputRate = vm["log-output-rate"].as<int>();
  }

  return true;
}

int main(int argc, char** argv)
{
  s_catch_signals();

  DeviceOptions_t options;
  try
  {
    if (!parse_cmd_line(argc, argv, &options))
      return 0;
  }
  catch (exception& e)
  {
    LOG(ERROR) << e.what();
    return 1;
  }
  
  // DDS
  // Construct the initial connection string.
  // Port will be changed after binding.
  std::string hostname(boost::asio::ip::host_name());
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query(hostname, "");
  boost::asio::ip::tcp::resolver::iterator it_begin = resolver.resolve(query);
  boost::asio::ip::tcp::resolver::iterator it_end;
  //for(auto it = it_begin; it != it_end;++it)
  //{
  // boost::asio::ip::tcp::endpoint ep = *it;
  //    std::cout << it->address() << ' ';
   //}

  // TODO we take the first resolved address
  boost::asio::ip::tcp::endpoint ep = *it_begin;

  std::stringstream ss;
  ss << "tcp://" << ep.address() << ":5655";
  std::string initialInputAddress = ss.str();
  //

  LOG(INFO) << "PID: " << getpid();

#ifdef NANOMSG
  FairMQTransportFactory* transportFactory = new FairMQTransportFactoryNN();
#else
  FairMQTransportFactory* transportFactory = new FairMQTransportFactoryZMQ();
#endif

  epn.SetTransport(transportFactory);

  epn.SetProperty(EPNex::Id, options.id);
  epn.SetProperty(EPNex::NumIoThreads, options.ioThreads);

  epn.SetProperty(EPNex::NumInputs, 1);
  epn.SetProperty(EPNex::NumOutputs, options.numOutputs);
  epn.SetProperty(EPNex::HeartbeatIntervalInMs, options.heartbeatIntervalInMs);

  epn.ChangeState(EPNex::INIT);

  epn.SetProperty(EPNex::InputSocketType, options.inputSocketType);
  epn.SetProperty(EPNex::InputRcvBufSize, options.inputBufSize);
  epn.SetProperty(EPNex::InputMethod, options.inputMethod);
  epn.SetProperty(EPNex::InputAddress, initialInputAddress);
  epn.SetProperty(EPNex::LogInputRate, options.logInputRate);

  //dds::CKeyValue::valuesMap_t::const_iterator it_values = values.begin();
  for (int i = 0; i < options.numOutputs; ++i)
  {
    epn.SetProperty(EPNex::OutputSocketType, options.outputSocketType, i);
    epn.SetProperty(EPNex::OutputSndBufSize, options.outputBufSize, i);
    epn.SetProperty(EPNex::OutputMethod, options.outputMethod, i);
    epn.SetProperty(EPNex::OutputAddress, "tcp://127.0.0.1:1234", i);
    epn.SetProperty(EPNex::LogOutputRate, options.logOutputRate, i);
	//it_values++;
  }

  epn.ChangeState(EPNex::SETOUTPUT);
  epn.ChangeState(EPNex::SETINPUT);
  epn.ChangeState(EPNex::BIND);
   
  dds::CKeyValue ddsKeyValue;
  
  ddsKeyValue.putValue("testEPNdistributedInputAddress", epn.GetProperty(EPNex::InputAddress, "", 0));
  
  // DDS
  // Waiting for properties
  dds::CKeyValue::valuesMap_t values;
  ddsKeyValue.getValues("heartbeatInputAddress", &values);
  while (values.size() != options.numOutputs)
  {
     ddsKeyValue.waitForUpdate(chrono::seconds(120));
     ddsKeyValue.getValues("heartbeatInputAddress", &values);
  }
  //
  dds::CKeyValue::valuesMap_t::const_iterator it_values = values.begin();
  for (int i = 0; i < options.numOutputs; ++i)
  {
    epn.SetProperty(EPNex::OutputAddress, it_values->second, i);
	it_values++;
  }
  //
  
  epn.ChangeState(EPNex::SETOUTPUT);
  epn.ChangeState(EPNex::SETINPUT);
  
  epn.ChangeState(EPNex::CONNECT);
  epn.ChangeState(EPNex::RUN);

  // wait until the running thread has finished processing.
  boost::unique_lock<boost::mutex> lock(epn.fRunningMutex);
  while (!epn.fRunningFinished)
  {
    epn.fRunningCondition.wait(lock);
  }

  epn.ChangeState(EPNex::STOP);
  epn.ChangeState(EPNex::END);

  return 0;
}
