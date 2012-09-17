#include <sstream>
#include <iomanip>

#include "object.h"

#include "CutGeometry.h"

MODULE_MAIN(CutGeometry)

CutGeometry::CutGeometry(int rank, int size, int moduleID)
   : Module("CutGeometry", rank, size, moduleID) {

   createInputPort("grid_in");
   createOutputPort("grid_out");
}

CutGeometry::~CutGeometry() {

}

vistle::Object * CutGeometry::cutGeometry(const vistle::Object * object,
                                          const vistle::Vector & point,
                                          const vistle::Vector & normal)
   const {

   if (object)
      switch (object->getType()) {

         case vistle::Object::SET: {
            const vistle::Set *in = static_cast<const vistle::Set *>(object);
            vistle::Set *out = vistle::Set::create();
            out->setBlock(in->getBlock());
            out->setTimestep(in->getTimestep());

            for (size_t index = 0; index < in->getNumElements(); index ++) {

               vistle::Object *o = cutGeometry(in->getElement(index),
                                               point, normal);
               if (o)
                  out->elements->push_back(o);
            }
            return out;
            break;
         }

         case vistle::Object::POLYGONS: {

            // mapping between vertex indices in the incoming object and
            // vertex indices in the outgoing object
            std::map<int, int> vertexMap;

            const vistle::Polygons *in =
               static_cast<const vistle::Polygons *>(object);
            vistle::Polygons *out = vistle::Polygons::create();
            out->setBlock(in->getBlock());
            out->setTimestep(in->getTimestep());

            const size_t *el = &((*in->el)[0]);
            const size_t *cl = &((*in->cl)[0]);
            const vistle::Scalar *x = &((*in->x)[0]);
            const vistle::Scalar *y = &((*in->y)[0]);
            const vistle::Scalar *z = &((*in->z)[0]);

            size_t numElements = in->getNumElements();
            for (size_t element = 0; element < numElements; element ++) {

               size_t start = el[element];
               size_t end;
               if (element != in->getNumElements() - 1)
                  end = el[element + 1] - 1;
               else
                  end = in->getNumCorners() - 1;

               size_t numIn = 0;

               for (size_t corner = start; corner <= end; corner ++) {
                  vistle::Vector p(x[cl[corner]],
                                         y[cl[corner]],
                                         z[cl[corner]]);
                  if ((p - point) * normal < 0)
                     numIn ++;
               }

               if (numIn == (end - start + 1)) {

                  // if all vertices in the element are on the right side
                  // of the cutting plane, insert the element and all vertices
                  out->el->push_back(out->cl->size());

                  for (size_t corner = start; corner <= end; corner ++) {

                     int vertexID = cl[corner];
                     int outID;

                     std::map<int, int>::iterator i =
                        vertexMap.find(vertexID);

                     if (i == vertexMap.end()) {
                        outID = out->x->size();
                        vertexMap[vertexID] = outID;
                        out->x->push_back(x[vertexID]);
                        out->y->push_back(y[vertexID]);
                        out->z->push_back(z[vertexID]);
                     } else
                        outID = i->second;

                     out->cl->push_back(outID);
                  }
               } else if (numIn > 0) {

                  // if not all of the vertices of an element are on the same
                  // side of the cutting plane:
                  //   - insert vertices that are on the right side of the plane
                  //   - omit vertices that are on the wrong side of the plane
                  //   - if the vertex before the processed vertex is on the
                  //     other side of the plane: insert the intersection point
                  //     between the line formed by the two vertices and the
                  //     plane
                  out->el->push_back(out->cl->size());

                  for (size_t corner = start; corner <= end; corner ++) {

                     int vertexID = cl[corner];

                     vistle::Vector p(x[cl[corner]],
                                            y[cl[corner]],
                                            z[cl[corner]]);

                     size_t former = (corner == start) ? end : corner - 1;
                     vistle::Vector pl(x[cl[former]],
                                             y[cl[former]],
                                             z[cl[former]]);

                     if (((p - point) * normal < 0 &&
                          (pl - point) * normal >= 0) ||
                         ((p - point) * normal >= 0 &&
                          (pl - point) * normal < 0)) {

                        vistle::Scalar s = (normal * (point - p)) /
                           (normal * (pl - p));
                        vistle::Vector pp = p + (pl - p) * s;

                        size_t outID = out->x->size();
                        out->x->push_back(pp.x);
                        out->y->push_back(pp.y);
                        out->z->push_back(pp.z);

                        out->cl->push_back(outID);
                     }

                     if ((p - point) * normal < 0) {

                        size_t outID;

                        std::map<int, int>::iterator i =
                           vertexMap.find(vertexID);

                        if (i == vertexMap.end()) {
                           outID = out->x->size();
                           vertexMap[vertexID] = outID;
                           out->x->push_back(x[vertexID]);
                           out->y->push_back(y[vertexID]);
                           out->z->push_back(z[vertexID]);
                        } else
                           outID = i->second;

                        out->cl->push_back(outID);
                     }
                  }
               }
            }
            return out;

            break;
         }

         default:
            break;
      }
   return NULL;
}

bool CutGeometry::compute() {

   vistle::Vector point(0.0, 0.0, 0.0);
   vistle::Vector normal(1.0, 0.0, 0.0);

   std::list<vistle::Object *> objects = getObjects("grid_in");
   while (objects.size()) {

      std::list<vistle::Object *>::iterator oit;
      for (oit = objects.begin(); oit != objects.end(); oit ++) {

         vistle::Object *object = cutGeometry(*oit, point, normal);
         if (object)
            addObject("grid_out", object);

         removeObject("grid_in", *oit);
      }
      objects = getObjects("grid_in");
   }
   return true;
}
