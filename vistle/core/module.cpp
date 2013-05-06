#include <mpi.h>

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include<Winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

#include <sstream>
#include <iostream>
#include <iomanip>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "object.h"
#include "message.h"
#include "messagequeue.h"
#include "parameter.h"
#include "module.h"
#include "shm.h"
#include "objectcache.h"

using namespace boost::interprocess;

namespace vistle {

Module::Module(const std::string &n, const std::string &shmname,
      const unsigned int r,
      const unsigned int s, const int m)
: m_name(n)
, m_rank(r)
, m_size(s)
, m_id(m)
, m_executionCount(0)
, m_defaultCacheMode(ObjectCache::CacheNone)
, m_mpiFinalize(true)
{

   message::DefaultSender::init(m_id, m_rank);

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

   const int HOSTNAMESIZE = 64;
   char hostname[HOSTNAMESIZE];
   gethostname(hostname, HOSTNAMESIZE - 1);

   std::cerr << "  module [" << name() << "] [" << id() << "] [" << rank()
             << "/" << size() << "] started as " << hostname << ":"
#ifndef _WIN32
             << getpid() << std::endl;
#else
             << std::endl;
#endif

   try {
      Shm::attach(shmname, id(), rank(), sendMessageQueue);

      std::string smqName =
         message::MessageQueue::createName("rmq", id(), rank());
      std::string rmqName =
         message::MessageQueue::createName("smq", id(), rank());

      sendMessageQueue = message::MessageQueue::open(smqName);
      receiveMessageQueue = message::MessageQueue::open(rmqName);

   } catch (interprocess_exception &ex) {
      std::cerr << "module " << id() << " [" << rank() << "/" << size() << "] "
                << ex.what() << std::endl;
      exit(2);
   }

   sendMessage(message::Started(name()));
}

const std::string &Module::name() const {

   return m_name;
}

int Module::id() const {

   return m_id;
}

unsigned int Module::rank() const {

   return m_rank;
}

unsigned int Module::size() const {

   return m_size;
}

void Module::setCacheMode(ObjectCache::CacheMode mode) {

   if (mode == ObjectCache::CacheDefault)
      m_cache.setCacheMode(m_defaultCacheMode);
   else
      m_cache.setCacheMode(mode);
}

void Module::setDefaultCacheMode(ObjectCache::CacheMode mode) {

   assert(mode != ObjectCache::CacheDefault);
   m_defaultCacheMode = mode;
   setCacheMode(m_defaultCacheMode);
}


ObjectCache::CacheMode Module::cacheMode(ObjectCache::CacheMode mode) const {

   return m_cache.cacheMode();
}

bool Module::createInputPort(const std::string &name, const std::string &description) {

   std::map<std::string, ObjectList>::iterator i = inputPorts.find(name);

   if (i == inputPorts.end()) {

      inputPorts[name] = ObjectList();

      message::CreateInputPort message(name);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }

   return false;
}

bool Module::createOutputPort(const std::string &name, const std::string &description) {

   std::map<std::string, ObjectList>::iterator i = outputPorts.find(name);

   if (i == outputPorts.end()) {

      outputPorts[name] = ObjectList();

      message::CreateOutputPort message(name);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }
   return false;
}

bool Module::addParameterGeneric(const std::string &name, Parameter *param, Parameter::Presentation presentation) {

   std::map<std::string, Parameter *>::iterator i =
      parameters.find(name);

   if (i != parameters.end())
      return false;

   parameters[name] = param;

   message::AddParameter add(name, param->description(), param->type(), presentation, m_name);
   sendMessage(add);
   message::SetParameter set(id(), name, param);
   sendMessage(set);

   return true;
}

bool Module::updateParameter(const std::string &name, const Parameter *param) {

   std::map<std::string, Parameter *>::iterator i =
      parameters.find(name);

   if (i == parameters.end()) {
      std::cerr << "setParameter: " << name << " not found" << std::endl;
      return false;
   }

   if (i->second->type() != param->type()) {
      std::cerr << "setParameter: type mismatch for " << name << " " << i->second->type() << " != " << param->type() << std::endl;
      return false;
   }

   if (&*i->second != param) {
      std::cerr << "setParameter: pointer mismatch for " << name << std::endl;
      return false;
   }

   message::SetParameter set(id(), name, param);
   sendMessage(set);

   return true;
}

template<class T>
bool Module::addParameter(const std::string &name, const std::string &description, const T &value, Parameter::Presentation pres) {

   Parameter *p = new ParameterBase<T>(name, value);
   p->setDescription(description);
   if (!addParameterGeneric(name, p, pres)) {
      delete p;
      return false;
   }
   return true;
}

template<class T>
bool Module::setParameter(const std::string &name, const T &value) {

   std::map<std::string, Parameter *>::iterator i =
      parameters.find(name);

   if (i == parameters.end())
      return false;

   ParameterBase<T> *p = dynamic_cast<ParameterBase<T> *>(&*i->second);
   if (!p)
      return false;

   p->setValue(value);
   return updateParameter(name, p);
}

template<class T>
bool Module::getParameter(const std::string &name, T &value) const {

   std::map<std::string, Parameter *>::const_iterator i =
      parameters.find(name);

   if (i == parameters.end())
      return false;

   if (i->second->type() != ParameterType<T>::type) {
      std::cerr << "Module::getParameter(" << name << "): type mismatch - expected: " << ParameterType<T>::type << ", actual: " << i->second->type() << std::endl;
      assert(i->second->type() == ParameterType<T>::type);
      return false;
   }

   if (ParameterBase<T> *p = dynamic_cast<ParameterBase<T> *>(&*i->second)) {
      value = p->getValue();
   } else {
      std::cerr << "Module::getParameter(" << name << "): type failure" << std::endl;
      assert("dynamic_cast failed" == 0);
      return false;
   }

   return true;
}

bool Module::addStringParameter(const std::string & name, const std::string &description,
                              const std::string & value, Parameter::Presentation p) {

   return addParameter(name, description, value, p);
}

bool Module::setStringParameter(const std::string & name,
                              const std::string & value) {

   return setParameter(name, value);
}

std::string Module::getStringParameter(const std::string & name) const {

   std::string value = "";
   getParameter(name, value);
   return value;
}

bool Module::addFloatParameter(const std::string & name, const std::string &description,
                               const vistle::Scalar value) {

   return addParameter(name, description, value);
}

bool Module::setFloatParameter(const std::string & name,
                               const vistle::Scalar value) {

   return setParameter(name, value);
}

vistle::Scalar Module::getFloatParameter(const std::string & name) const {

   Scalar value = 0.;
   getParameter(name, value);
   return value;
}

bool Module::addIntParameter(const std::string & name, const std::string &description,
                             const int value, Parameter::Presentation p) {

   return addParameter(name, description, value, p);
}

bool Module::setIntParameter(const std::string & name,
                             const int value) {

   return setParameter(name, value);
}

int Module::getIntParameter(const std::string & name) const {

   int value = 0;
   getParameter(name, value);
   return value;
}

bool Module::addVectorParameter(const std::string & name, const std::string &description,
                                const ParamVector & value) {

   return addParameter(name, description, value);
}

bool Module::setVectorParameter(const std::string & name,
                                const ParamVector & value) {

   return setParameter(name, value);
}

ParamVector Module::getVectorParameter(const std::string & name) const {

   ParamVector value;
   getParameter(name, value);
   return value;
}

void Module::updateMeta(vistle::Object::ptr obj) const {

   obj->setCreator(id());
   obj->setExecutionCounter(m_executionCount);
}

bool Module::addObject(const std::string &portName, vistle::Object::ptr object) {

   updateMeta(object);
   vistle::Object::const_ptr cobj = object;
   return passThroughObject(portName, cobj);
}

bool Module::passThroughObject(const std::string & portName, vistle::Object::const_ptr object) {

   if (!object)
      return false;

   assert(object->check());

   std::map<std::string, ObjectList>::iterator i = outputPorts.find(portName);
   if (i != outputPorts.end()) {
      // XXX: this was the culprit keeping the final object reference around
      //i->second.push_back(object);
      message::AddObject message(portName, object);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }
   std::cerr << "Module::passThroughObject: output port " << portName << " not found" << std::endl;
   assert(i != outputPorts.end());
   return false;
}

ObjectList Module::getObjects(const std::string &portName) {

   ObjectList objects;
   std::map<std::string, ObjectList>::iterator i = inputPorts.find(portName);

   if (i != inputPorts.end()) {

      std::list<shm_handle_t>::iterator shmit;
      for (ObjectList::const_iterator it = i->second.begin(); it != i->second.end(); it++) {
         Object::const_ptr object = *it;
         if (object.get())
            assert(object->check());
            objects.push_back(object);
      }
   } else {
      std::cerr << "Module::getObjects: input port " << portName << " not found" << std::endl;
      assert(i != inputPorts.end());
   }

   return objects;
}

void Module::removeObject(const std::string &portName, vistle::Object::const_ptr object) {

   bool erased = false;
   shm_handle_t handle = object->getHandle();
   std::map<std::string, ObjectList>::iterator i = inputPorts.find(portName);

   if (i != inputPorts.end()) {
      ObjectList &olist = i->second;

      for (ObjectList::iterator it = olist.begin(); it != olist.end(); ) {
         if (handle == (*it)->getHandle()) {
            erased = true;
            //object->unref(); // XXX: doesn't erasing the it handle that already?
            it = olist.erase(it);
         } else
            ++it;
      }
      if (!erased)
         std::cerr << "Module " << id() << " removeObject didn't find"
            " object [" << object->getName() << "]" << std::endl;
   } else {
      std::cerr << "Module " << id() << " removeObject didn't find port ["
                << portName << "]" << std::endl;

      assert(i != inputPorts.end());
   }
}

bool Module::hasObject(const std::string &portName) const {

   std::map<std::string, ObjectList>::const_iterator i = inputPorts.find(portName);

   if (i != inputPorts.end()) {

      return !i->second.empty();
   }

   std::cerr << "Module::hasObject: input port " << portName << " not found" << std::endl;
   assert(i != inputPorts.end());

   return false;
}

vistle::Object::const_ptr Module::takeFirstObject(const std::string &portName) {

   std::map<std::string, ObjectList>::iterator i = inputPorts.find(portName);

   if (i != inputPorts.end() && !i->second.empty()) {

      Object::const_ptr obj = i->second.front();
      assert(obj->check());
      i->second.pop_front();
      return obj;
   }

   std::cerr << "Module::takeFirstObject: input port " << portName << " not found" << std::endl;
   assert(i != inputPorts.end());

   return vistle::Object::ptr();
}

bool Module::addInputObject(const std::string & portName,
                            Object::const_ptr object) {

   if (!object)
      return false;

   if (object)
      assert(object->check());

   if (m_executionCount < object->getExecutionCounter())
      m_executionCount = object->getExecutionCounter();

   std::map<std::string, ObjectList>::iterator i = inputPorts.find(portName);

   if (i != inputPorts.end()) {
      m_cache.addObject(portName, object);
      i->second.push_back(object);
      return true;
   }

   std::cerr << "Module::addInputObject: input port " << portName << " not found" << std::endl;
   assert(i != inputPorts.end());

   return false;
}

bool Module::parameterAdded(const int senderId, const std::string &name, const message::AddParameter &msg, const std::string &moduleName) {

   (void)senderId;
   (void)name;
   (void)msg;
   (void)moduleName;
   return false;
}

bool Module::parameterChanged(const int senderId, const std::string &name, const message::SetParameter &msg) {

   (void)senderId;
   (void)name;
   (void)msg;
   return false;
}


bool Module::dispatch() {

   size_t msgSize;
   unsigned int priority;
   char msgRecvBuf[message::Message::MESSAGE_SIZE];

   receiveMessageQueue->getMessageQueue().receive(
                                               (void *) msgRecvBuf,
                                               message::Message::MESSAGE_SIZE,
                                               msgSize, priority);

   vistle::message::Message *message = (vistle::message::Message *) msgRecvBuf;

   bool again = handleMessage(message);
   return again;
}


void Module::sendMessage(const message::Message &message) const {

   sendMessageQueue->getMessageQueue().send(&message, message.size(), 0);
}


bool Module::handleMessage(const vistle::message::Message *message) {

   switch (message->type()) {

      case vistle::message::Message::PING: {

         const vistle::message::Ping *ping =
            static_cast<const vistle::message::Ping *>(message);

         std::cerr << "    module [" << name() << "] [" << id() << "] ["
                   << rank() << "/" << size() << "] ping ["
                   << ping->getCharacter() << "]" << std::endl;
         vistle::message::Pong m(ping->getCharacter(), ping->senderId());
         sendMessage(m);
         break;
      }

      case vistle::message::Message::PONG: {

         const vistle::message::Pong *pong =
            static_cast<const vistle::message::Pong *>(message);

         std::cerr << "    module [" << name() << "] [" << id() << "] ["
                   << rank() << "/" << size() << "] pong ["
                   << pong->getCharacter() << "]" << std::endl;
         break;
      }

      case message::Message::QUIT: {

         const message::Quit *quit =
            static_cast<const message::Quit *>(message);
         (void) quit;
         return false;
         break;
      }

      case message::Message::KILL: {

         const message::Kill *kill =
            static_cast<const message::Kill *>(message);
         if (kill->getModule() == id()) {
            return false;
         } else {
            std::cerr << "module [" << name() << "] [" << id() << "] ["
               << rank() << "/" << size() << "]" << ": received invalid Kill message" << std::endl;
         }
         break;
      }

      case message::Message::COMPUTE: {

         const message::Compute *comp =
            static_cast<const message::Compute *>(message);
         if (m_executionCount < comp->getExecutionCount())
            m_executionCount = comp->getExecutionCount();

         if (comp->getExecutionCount() > 0) {
            for (std::map<std::string, ObjectList>::iterator pit = inputPorts.begin();
                  pit != inputPorts.end();
                  ++pit) {
               pit->second = m_cache.getObjects(pit->first);
            }
         }
         /*
         std::cerr << "    module [" << name() << "] [" << id() << "] ["
                   << rank() << "/" << size << "] compute" << std::endl;
         */
         message::Busy busy;
         sendMessage(busy);
         bool ret = false;
         try {
            ret = compute();
         } catch (std::exception &e) {
            std::cerr << name() << ": exception - " << e.what() << std::endl;
         }
         message::Idle idle;
         sendMessage(idle);
         return ret;
         break;
      }

      case message::Message::ADDOBJECT: {

         const message::AddObject *add =
            static_cast<const message::AddObject *>(message);
         addInputObject(add->getPortName(), add->takeObject());
         break;
      }

      case message::Message::SETPARAMETER: {

         const message::SetParameter *param =
            static_cast<const message::SetParameter *>(message);

         if (param->getModule() == id()) {

            // sent by controller
            switch (param->getParameterType()) {
               case Parameter::Integer:
                  setIntParameter(param->getName(), param->getInteger());
                  break;
               case Parameter::Scalar:
                  setFloatParameter(param->getName(), param->getScalar());
                  break;
               case Parameter::Vector:
                  setVectorParameter(param->getName(), param->getVector());
                  break;
               case Parameter::String:
                  setStringParameter(param->getName(), param->getString());
                  break;
            }
         } else {

            parameterChanged(param->senderId(), param->getName(), *param);
         }
         break;
      }

      case message::Message::ADDPARAMETER: {

         const message::AddParameter *param =
            static_cast<const message::AddParameter *>(message);

         parameterAdded(param->senderId(), param->getName(), *param, param->moduleName());
         break;
      }

      case message::Message::BARRIER: {

         const message::Barrier *barrier =
            static_cast<const message::Barrier *>(message);
         sendMessage(message::BarrierReached(barrier->getBarrierId()));
         break;
      }

      default:
         std::cerr << "    module [" << name() << "] [" << id() << "] ["
                   << rank() << "/" << size() << "] unknown message type ["
                   << message->type() << "]" << std::endl;

         break;
   }

   return true;
}

Module::~Module() {

   vistle::message::ModuleExit m;
   sendMessage(m);

   std::cerr << "  module [" << name() << "] [" << id() << "] [" << rank()
             << "/" << size() << "] quit" << std::endl;

   MPI_Barrier(MPI_COMM_WORLD);

   if (m_mpiFinalize)
      MPI_Finalize();
}

} // namespace vistle
