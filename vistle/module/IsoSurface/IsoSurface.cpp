﻿//
//This code is used for both IsoCut and IsoSurface!
//

#include <iostream>
#include <cmath>
#include <ctime>
#include <boost/mpi/collectives.hpp>
#include <core/message.h>
#include <core/object.h>
#include <core/unstr.h>
#include <core/vec.h>
#include "IsoSurface.h"
#include "Leveller.h"

MODULE_MAIN(IsoSurface)

using namespace vistle;

IsoSurface::IsoSurface(const std::string &shmname, const std::string &name, int moduleID)
   : Module(
#ifdef CUTTINGSURFACE
"cut through grids"
#else
"extract isosurfaces"
#endif
, shmname, name, moduleID) {

   setDefaultCacheMode(ObjectCache::CacheAll);
   setReducePolicy(message::ReducePolicy::OverAll);
   createInputPort("grid_in");
#ifdef CUTTINGSURFACE
   m_mapDataIn = createInputPort("data_in");
   addVectorParameter("point", "point on plane", ParamVector(0.0, 0.0, 0.0));
   addVectorParameter("vertex", "normal on plane", ParamVector(1.0, 0.0, 0.0));
   addFloatParameter("scalar", "distance to origin of ordinates", 0.0);
   addVectorParameter("direction", "Direction for variable Cylinder", ParamVector(0.0, 0.0, 0.0));
#else
   createInputPort("data_in");
   m_mapDataIn = createInputPort("mapdata_in");
#endif    
   createOutputPort("grid_out");
   m_processortype = addIntParameter("processortype", "processortype", 0, Parameter::Choice);
   V_ENUM_SET_CHOICES(m_processortype, ThrustBackend);
#ifdef CUTTINGSURFACE
   m_mapDataOut = createOutputPort("data_out");
   m_option = addIntParameter("option", "option", 0, Parameter::Choice);
   V_ENUM_SET_CHOICES(m_option, SurfaceOption);
#else
   m_mapDataOut = createOutputPort("mapdata_out");
   m_isovalue = addFloatParameter("isovalue", "isovalue", 0.0);
#endif

}

IsoSurface::~IsoSurface() {

}

bool IsoSurface::parameterChanged(const Parameter* param) {
#ifdef CUTTINGSURFACE
   switch(m_option->getValue()) {
      case Plane: {
         if(param->getName() == "point") {
            Vector vertex = getVectorParameter("vertex");
            Vector point = getVectorParameter("point");
            setFloatParameter("scalar",point.dot(vertex));
         }
         break;
      }
      case Sphere: {
         if(param->getName() == "point") {
            Vector vertex = getVectorParameter("vertex");
            Vector point = getVectorParameter("point");
            Vector diff = vertex - point;
            setFloatParameter("scalar",diff.norm());
         }break;
      }
      default:/*cylinders*/ {
         if(param->getName() == "option") {
            switch(m_option->getValue()) {

               case CylinderX:
                  setVectorParameter("direction",ParamVector(1,0,0));
                  break;
               case CylinderY:
                  setVectorParameter("direction",ParamVector(0,1,0));
                  break;
               case CylinderZ:
                  setVectorParameter("direction",ParamVector(0,0,1));
                  break;
            }
         }
         if(param->getName() == "point") {
            std::cerr<< "point" << std::endl;
            Vector vertex = getVectorParameter("vertex");
            Vector point = getVectorParameter("point");
            Vector direction = getVectorParameter("direction");
            Vector diff = direction.cross(vertex-point);
            setFloatParameter("scalar",diff.norm());
         }
         if(param->getName() == "scalar") {
            std::cerr<< "scalar" << std::endl;
            Vector vertex = getVectorParameter("vertex");
            Vector point = getVectorParameter("point");
            Vector direction = getVectorParameter("direction");
            Vector diff = direction.cross(vertex-point);
            float scalar = getFloatParameter("scalar");
            Vector newPoint = scalar*diff/diff.norm();
            setVectorParameter("point",ParamVector(newPoint[0], newPoint[1], newPoint[2]));
         }
         break;
      }
   }
#endif
   return true;
}

bool IsoSurface::prepare() {

   m_min = std::numeric_limits<Scalar>::max() ;
   m_max = -std::numeric_limits<Scalar>::max();
   return Module::prepare();
}

bool IsoSurface::reduce(int timestep) {

#ifndef CUTTINGSURFACE
   Scalar min, max;
   boost::mpi::all_reduce(boost::mpi::communicator(),
                          m_min, min, boost::mpi::minimum<Scalar>());
   boost::mpi::all_reduce(boost::mpi::communicator(),
                          m_max, max, boost::mpi::maximum<Scalar>());

   setParameterRange(m_isovalue, (Float)min, (Float)max);
#endif

   return Module::reduce(timestep);
}

bool IsoSurface::compute() {

   const int processorType = getIntParameter("processortype");
#ifdef CUTTINGSURFACE
   const Scalar isoValue = 0.0;
   int option = getIntParameter("option");
   const Vector vertex = getVectorParameter("vertex");
   const Vector point = getVectorParameter("point");
   const Vector direction = getVectorParameter("direction");
#else
   const Scalar isoValue = getFloatParameter("isovalue");
#endif

   auto gridS = expect<UnstructuredGrid>("grid_in");
   if (!gridS)
      return false;

#ifndef CUTTINGSURFACE
   auto dataS = expect<Vec<Scalar>>("data_in");
   if (!dataS)
      return false;
#endif

   Leveller l(gridS, isoValue, processorType
#ifdef CUTTINGSURFACE
              , option
#endif
              );

#ifdef CUTTINGSURFACE
   l.setCutData(vertex,point, direction);
#else
   l.setIsoData(dataS);
#endif
   auto mapdata = accept<Object>(m_mapDataIn);
   if(mapdata){
      l.addMappedData(mapdata);
   };
   l.process();

#ifndef CUTTINGSURFACE
#if 0
   auto range = l.range();
   if (range.first < m_min)
      m_min = range.first;
   if (range.second > m_max)
      m_max = range.second;
#else
   auto minmax = dataS->getMinMax();
   if (minmax.first[0] < m_min)
      m_min = minmax.first[0];
   if (minmax.second[0] > m_max)
      m_max = minmax.second[0];
#endif
#endif

   Object::ptr result = l.result();
   Object::ptr mapresult = l.mapresult();
   if (result && !result->isEmpty()) {
#ifndef CUTTINGSURFACE
      result->copyAttributes(dataS);
#endif
      result->copyAttributes(gridS, false);
      addObject("grid_out", result);
      if(!Empty::as(mapresult)) {
         mapresult->copyAttributes(mapdata);
      }
      addObject(m_mapDataOut, mapresult);
   }
   return true;
}
