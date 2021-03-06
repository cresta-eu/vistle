#ifndef OBJECT_IMPL_H
#define OBJECT_IMPL_H

#include "archives.h"
#include "serialize.h"

BOOST_CLASS_IMPLEMENTATION(vistle::shm<char>::string, boost::serialization::primitive_type)

namespace boost {
namespace serialization {

template<>
void access::destroy(const vistle::shm<char>::string *t);

template<>
void access::construct(vistle::shm<char>::string *t);

template<>
void access::destroy(const vistle::Object::Data::AttributeList *t);

template<>
void access::construct(vistle::Object::Data::AttributeList *t);

template<>
void access::destroy(const vistle::Object::Data::AttributeMapValueType *t);

template<>
void access::construct(vistle::Object::Data::AttributeMapValueType *t);

#if 0
template<>
void access::destroy(const vistle::Object *t);

template<>
void access::construct(vistle::Object *t);
#endif

} // namespace serialization
} // namespace boost


namespace vistle {

template<class Archive>
void Object::Data::serialize(Archive &ar, const unsigned int version) {
#ifdef DEBUG_SERIALIZATION
   int checktype1 = type;
   ar & V_NAME("checktype1", checktype1);
   if (type != checktype1) {
      std::cerr << "typecheck 1: should be " << type << ", read " << checktype1 << std::endl;
   }
   assert(checktype1 == type);
#endif
   ar & V_NAME("meta", meta);
   ar & V_NAME("attributes", *attributes);
#ifdef DEBUG_SERIALIZATION
   int checktype2 = type;
   ar & V_NAME("checktype2", checktype2);
   if (type != checktype2) {
      std::cerr << "typecheck 2: should be " << type << ", read " << checktype2 << std::endl;
   }
   assert(checktype2 == type);
#endif
}

template<class Archive>
void Object::save(Archive &ar) const {

   ObjectTypeRegistry::registerArchiveType(ar);
   const Object *p = this;
   ar & V_NAME("object", p);
}

template<class Archive>
Object::ptr Object::load(Archive &ar) {

   ObjectTypeRegistry::registerArchiveType(ar);
   Object *p = NULL;
   ar & V_NAME("object", p);
   return Object::ptr(p);
}

template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::text_iarchive &ar);
template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::text_oarchive &ar);
template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::binary_iarchive &ar);
template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::binary_oarchive &ar);
template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::xml_iarchive &ar);
template<>
V_COREEXPORT void ObjectTypeRegistry::registerArchiveType(boost::archive::xml_oarchive &ar);

} // namespace vistle

#endif
