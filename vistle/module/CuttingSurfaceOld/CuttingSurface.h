#ifndef CUTTINGSURFACE_H
#define CUTTINGSURFACE_H

#include <module/module.h>
#include <core/vector.h>

class CuttingSurface: public vistle::Module {

 public:
   CuttingSurface(const std::string &shmname, const std::string &name, int moduleID);
   ~CuttingSurface();

 private:
   std::pair<vistle::Object::ptr, vistle::Object::ptr>
      generateCuttingSurface(vistle::Object::const_ptr grid,
                             vistle::Object::const_ptr data,
                             const vistle::Vector & normal,
                             const vistle::Scalar distance);

   virtual bool compute();
};

#endif
