#include "texture1d.h"

namespace vistle {

Texture1D::Texture1D(const size_t width,
      const Scalar min, const Scalar max,
      const int block, const int timestep)
: Texture1D::Base(Texture1D::Data::create(width, min, max, block, timestep))
{
}

Texture1D::Data::Data(const std::string &name, const size_t width,
                     const Scalar mi, const Scalar ma,
                     const int block, const int timestep)
   : Texture1D::Base::Data(Object::TEXTURE1D, name, block, timestep)
   , min(mi)
   , max(ma)
   , pixels(new ShmVector<unsigned char>(width * 4))
   , coords(new ShmVector<Scalar>(1))
{

#if  0
   const allocator<Scalar, managed_shared_memory::segment_manager>
      alloc_inst_Scalar(Shm::instance().getShm().get_segment_manager());

   coords = Shm::instance().getShm().construct<shm<Scalar>::vector>(Shm::instance().createObjectID().c_str())(alloc_inst_Scalar);
#endif
}

Texture1D::Data *Texture1D::Data::create(const size_t width,
                              const Scalar min, const Scalar max,
                              const int block, const int timestep) {

   const std::string name = Shm::instance().createObjectID();
   Data *tex= shm<Data>::construct(name)(name, width, min, max, block, timestep);
   publish(tex);

   return tex;
}

size_t Texture1D::getNumElements() const {

   return d()->coords->size();
}

size_t Texture1D::getWidth() const {

   return d()->pixels->size() / 4;
}

V_OBJECT_TYPE(Texture1D, Object::TEXTURE1D);

} // namespace vistle
