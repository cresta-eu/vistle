#ifndef ADD_H
#define ADD_H

#include "module.h"

class Add: public vistle::Module {

 public:
   Add(int rank, int size, int moduleID);
   ~Add();

 private:
   virtual bool compute();
};

#endif