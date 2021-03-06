#ifndef VEC_H
#define VEC_H

#include "index.h"
#include "dimensions.h"
#include "shm.h"
#include "object.h"
#include "vector.h"


namespace vistle {

template <typename T, int Dim=1>
class Vec: public Object {
   V_OBJECT(Vec);

   static const int MaxDim = MaxDimension;
   BOOST_STATIC_ASSERT(Dim > 0);
   BOOST_STATIC_ASSERT(Dim <= MaxDim);

 public:
   typedef Object Base;
   typedef typename shm<T>::array array;
   typedef T Scalar;
   typedef typename VistleScalarVector<Dim>::type Vector;

   Vec(const Index size,
        const Meta &meta=Meta());

   Index getSize() const {
      return d()->x[0]->size();
   }

   void setSize(const Index size);

   array &x(int c=0) const { return *(*d()->x[c])(); }
   array &y() const { return *(*d()->x[1])(); }
   array &z() const { return *(*d()->x[2])(); }
   array &w() const { return *(*d()->x[3])(); }

   std::pair<Vector, Vector> getMinMax() const;

   struct Data: public Base::Data {

      typename ShmVector<T>::ptr x[Dim];
      // when used as Vec
      Data(const Index size = 0, const std::string &name = "",
            const Meta &meta=Meta());
      // when used as base of another data structure
      Data(const Index size, Type id, const std::string &name,
            const Meta &meta=Meta());
      Data(const Data &other, const std::string &name, Type id=UNKNOWN);
      static Data *create(Index size = 0, const Meta &meta=Meta());

      private:
      friend class Vec;
      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive &ar, const unsigned int version);
   };
};

} // namespace vistle

#ifdef VISTLE_IMPL
#include "vec_impl.h"
#endif
#endif
