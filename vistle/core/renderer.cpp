#include "message.h"
#include "messagequeue.h"

#include "renderer.h"

namespace vistle {

Renderer::Renderer(const std::string & name,
                   const int rank, const int size, const int moduleID)
   : Module(name, rank, size, moduleID) {

   createInputPort("data_in");
}

Renderer::~Renderer() {

}

bool Renderer::dispatch() {

   size_t msgSize;
   unsigned int priority;
   char msgRecvBuf[128];

   bool done = false;
   bool msg =
      receiveMessageQueue->getMessageQueue().try_receive((void *) msgRecvBuf,
                                                         (size_t) 128, msgSize,
                                                         priority);

   if (msg) {
      vistle::message::Message *message =
         (vistle::message::Message *) msgRecvBuf;
      done = handleMessage(message);

      if (done) {
         vistle::message::ModuleExit m(moduleID, rank);
         sendMessageQueue->getMessageQueue().send(&m, sizeof(m), 0);
      }
   }

   if (!done)
      render();

   return done;
}

} // namespace vistle