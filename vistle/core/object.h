#ifndef OBJECT_H
#define OBJECT_H

#include <vector>
#include <boost/shared_ptr.hpp>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>

#include <boost/serialization/access.hpp>

#include "shm.h"
#include "export.h"

namespace boost {
namespace archive {

class text_iarchive;
class text_oarchive;
class xml_oarchive;
class xml_iarchive;
class binary_oarchive;
class binary_iarchive;


} // namespace archive
} // namespace boost


namespace vistle {

namespace bi = boost::interprocess;

typedef bi::managed_shared_memory::handle_t shm_handle_t;

class Shm;

class V_COREEXPORT Meta {
   public:
      Meta(int timestep=-1, int block=-1, int animationstep=-1, int iteration=-1, int execcount=-1, int creator=-1);
      int block() const { return m_block; }
      void setBlock(int block) { m_block = block; }
      int numBlocks() const { return m_numBlocks; }
      void setNumBlocks(int num) { m_numBlocks = num; }
      int timeStep() const { return m_timestep; }
      void setTimeStep(int timestep) { m_timestep = timestep; }
      int numTimesteps() const { return m_numTimesteps; }
      void setNumTimesteps(int num) { m_numTimesteps = num; }
      int animationStep() const { return m_animationstep; }
      void setAnimationStep(int step) { m_animationstep = step; }
      int iteration() const { return m_iteration; }
      void setIteration(int iteration) { m_iteration = iteration; }
      int executionCounter() const { return m_executionCount; }
      void setExecutionCounter(int count) { m_executionCount = count; }
      int creator() const { return m_creator; }
      void setCreator(int id) { m_creator = id; }
   private:
      int m_block, m_numBlocks, m_timestep, m_numTimesteps, m_animationstep, m_iteration, m_executionCount, m_creator;

      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive &ar, const unsigned int version);
};

#define V_NAME(name, obj) \
   boost::serialization::make_nvp(name, (obj))

class V_COREEXPORT Object {
   friend class Shm;
   friend class ObjectTypeRegistry;
#ifdef SHMDEBUG
   friend void Shm::markAsRemoved(const std::string &name);
#endif

public:
   typedef boost::shared_ptr<Object> ptr;
   typedef boost::shared_ptr<const Object> const_ptr;
   
   enum InitializedFlags {
      Initialized
   };

   enum Type {
      UNKNOWN           = -1,

      TEXTURE1D         = 17,
      GEOMETRY          = 18,

      POINTS            = 19,
      LINES             = 20,
      TRIANGLES         = 21,
      POLYGONS          = 22,
      UNSTRUCTUREDGRID  = 23,

      VEC               = 100, // base type id for all Vec types
   };

   virtual ~Object();

   virtual Object::ptr clone() const = 0;

   virtual bool check() const;

   template<class ObjectType>
   static boost::shared_ptr<const Object> as(boost::shared_ptr<const ObjectType> ptr) { return boost::static_pointer_cast<const Object>(ptr); }
   template<class ObjectType>
   static boost::shared_ptr<Object> as(boost::shared_ptr<ObjectType> ptr) { return boost::static_pointer_cast<Object>(ptr); }

   shm_handle_t getHandle() const;

   Type getType() const;
   std::string getName() const;

   int getBlock() const;
   int getTimestep() const;
   int getExecutionCounter() const;
   int getCreator() const;

   void setBlock(const int block);
   void setNumBlocks(const int num);
   void setTimestep(const int timestep);
   void setNumTimesteps(const int num);
   void setExecutionCounter(const int count);
   void setCreator(const int id);

   const Meta &meta() const;
   void setMeta(const Meta &meta);

   void addAttribute(const std::string &key, const std::string &value = "");
   void setAttributeList(const std::string &key, const std::vector<std::string> &values);
   void copyAttributes(Object::const_ptr src, bool replace = true);
   bool hasAttribute(const std::string &key) const;
   std::string getAttribute(const std::string &key) const;
   std::vector<std::string> getAttributes(const std::string &key) const;

   void ref() const;
   void unref() const;
   int refcount() const;

   template<class Archive>
   static Object::ptr load(Archive &ar);

   template<class Archive>
   void save(Archive &ar) const;

 protected:
   struct Data {
      Type type;
      shm_name_t name;
      int refcount;
      boost::interprocess::interprocess_mutex mutex;

      Meta meta;

      typedef shm<char>::string Attribute;
      typedef shm<char>::string Key;
      typedef shm<Attribute>::vector AttributeList;
      typedef std::pair<const Key, AttributeList> AttributeMapValueType;
      typedef shm<AttributeMapValueType>::allocator AttributeMapAllocator;
      typedef bi::map<Key, AttributeList, std::less<Key>, AttributeMapAllocator> AttributeMap;
      bi::offset_ptr<AttributeMap> attributes;
      void addAttribute(const std::string &key, const std::string &value = "");
      void setAttributeList(const std::string &key, const std::vector<std::string> &values);
      void copyAttributes(const Data *src, bool replace);
      bool hasAttribute(const std::string &key) const;
      std::string getAttribute(const std::string &key) const;
      std::vector<std::string> getAttributes(const std::string &key) const;

      Data(Type id = UNKNOWN, const std::string &name = "", const Meta &m=Meta());
      Data(const Data &other, const std::string &name); //! shallow copy, except for attributes
      ~Data();
      void *operator new(size_t size);
      void *operator new (std::size_t size, void* ptr);
      void operator delete(void *ptr);
      void ref();
      void unref();
      static Data *create(Type id, const Meta &m);

      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive &ar, const unsigned int version);

      // not implemented
      Data(const Data &);
      Data &operator=(const Data &);
   };

   Data *m_data;
 public:
   Data *d() const { return m_data; }
 protected:

   Object(Data *data);
   Object();

   static void publish(const Data *d);
   static Object::ptr create(Data *);
 private:
   friend class boost::serialization::access;
   template<class Archive>
      void serialize(Archive &ar, const unsigned int version) {
         d()->serialize<Archive>(ar, version);
      }
   // not implemented
   Object(const Object &);
   Object &operator=(const Object &);
};

class V_COREEXPORT ObjectTypeRegistry {
   friend struct Object::Data;
   friend Object::ptr Object::create(Object::Data *);
   public:
   typedef Object::ptr (*CreateFunc)(Object::Data *d);
   typedef void (*DestroyFunc)(const std::string &name);
   typedef void (*RegisterTextIArchiveFunc)(boost::archive::text_iarchive &ar);
   typedef void (*RegisterTextOArchiveFunc)(boost::archive::text_oarchive &ar);
   typedef void (*RegisterXmlIArchiveFunc)(boost::archive::xml_iarchive &ar);
   typedef void (*RegisterXmlOArchiveFunc)(boost::archive::xml_oarchive &ar);
   typedef void (*RegisterBinaryIArchiveFunc)(boost::archive::binary_iarchive &ar);
   typedef void (*RegisterBinaryOArchiveFunc)(boost::archive::binary_oarchive &ar);

   template<class O>
   static void registerType(int id) {
      assert(typeMap().find(id) == typeMap().end());
      struct FunctionTable t = {
         O::createFromData,
         O::destroy,
         O::registerBinaryIArchive,
         O::registerBinaryOArchive,
         O::registerTextIArchive,
         O::registerTextOArchive,
         O::registerXmlIArchive,
         O::registerXmlOArchive,
      };
      typeMap()[id] = t;
   }

   template<class Archive>
   static void registerArchiveType(Archive &ar);

   private:
   struct FunctionTable {
      CreateFunc create;
      DestroyFunc destroy;
      RegisterBinaryIArchiveFunc registerBinaryIArchive;
      RegisterBinaryOArchiveFunc registerBinaryOArchive;
      RegisterTextIArchiveFunc registerTextIArchive;
      RegisterTextOArchiveFunc registerTextOArchive;
      RegisterXmlIArchiveFunc registerXmlIArchive;
      RegisterXmlOArchiveFunc registerXmlOArchive;
   };
   static const struct FunctionTable &getType(int id);
   typedef std::map<int, FunctionTable> TypeMap;

   static TypeMap &typeMap();
   static CreateFunc getCreator(int id);
   static DestroyFunc getDestroyer(int id);
};

//! use in checkImpl
#define V_CHECK(true_expr) \
   if (!(true_expr)) { \
      std::cerr << __FILE__ << ":" << __LINE__ << ": " \
      << "CONSISTENCY CHECK FAILURE: " \
          << #true_expr << std::endl; \
      std::cerr << "   PID: " << getpid() << std::endl; \
      sleep(30); \
      return false; \
   }

//! declare a new Object type
#define V_OBJECT(Type) \
   public: \
   typedef boost::shared_ptr<Type> ptr; \
   typedef boost::shared_ptr<const Type> const_ptr; \
   typedef Type Class; \
   static boost::shared_ptr<const Type> as(boost::shared_ptr<const Object> ptr) { return boost::dynamic_pointer_cast<const Type>(ptr); } \
   static boost::shared_ptr<Type> as(boost::shared_ptr<Object> ptr) { return boost::dynamic_pointer_cast<Type>(ptr); } \
   static Object::ptr createFromData(Object::Data *data) { return Object::ptr(new Type(static_cast<Type::Data *>(data))); } \
   Object::ptr clone() const { \
      const std::string n(Shm::the().createObjectID()); \
            Data *data = shm<Data>::construct(n)(*d(), n); \
            publish(data); \
            return createFromData(data); \
   } \
   static void destroy(const std::string &name) { shm<Type::Data>::destroy(name); } \
   static void registerTextIArchive(boost::archive::text_iarchive &ar); \
   static void registerTextOArchive(boost::archive::text_oarchive &ar); \
   static void registerXmlIArchive(boost::archive::xml_iarchive &ar); \
   static void registerXmlOArchive(boost::archive::xml_oarchive &ar); \
   static void registerBinaryIArchive(boost::archive::binary_iarchive &ar); \
   static void registerBinaryOArchive(boost::archive::binary_oarchive &ar); \
   Type(Object::InitializedFlags) : Base(Type::Data::create()) {} \
   bool check() const { if (!Base::check()) return false; return checkImpl(); } \
   protected: \
   bool checkImpl() const; \
   struct Data; \
   Data *d() const { return static_cast<Data *>(Object::m_data); } \
   Type(Data *data) : Base(data) {} \
   Type() : Base() {} \
   private: \
   friend class boost::serialization::access; \
   template<class Archive> \
      void serialize(Archive &ar, const unsigned int version) { \
         boost::serialization::split_member(ar, *this, version); \
      } \
   template<class Archive> \
      void load(Archive &ar, const unsigned int version) { \
         int type = Object::UNKNOWN; \
         ar & V_NAME("type", type); \
         Object::m_data = Data::create(); \
         Object::ref(); \
         d()->template serialize<Archive>(ar, version); \
         assert(type == Object::getType()); \
      } \
   template<class Archive> \
      void save(Archive &ar, const unsigned int version) const { \
         int type = Object::getType(); \
         ar & V_NAME("type", type); \
         d()->template serialize<Archive>(ar, version); \
      } \
   friend boost::shared_ptr<const Object> Shm::getObjectFromHandle(const shm_handle_t &); \
   friend shm_handle_t Shm::getHandleFromObject(boost::shared_ptr<const Object>); \
   friend boost::shared_ptr<Object> Object::create(Object::Data *); \
   friend class ObjectTypeRegistry

#define V_DATA_BEGIN(Type) \
   protected: \
   struct Data: public Base::Data { \
      Data(const Data &other, const std::string &name)

#define V_DATA_END(Type) \
      private: \
      friend class Type; \
      friend class boost::serialization::access; \
      template<class Archive> \
      void serialize(Archive &ar, const unsigned int version); \
   }

#define V_SERIALIZERS4(Type1, Type2, prefix1, prefix2) \
   prefix1, prefix2 \
   void Type1, Type2::registerTextIArchive(boost::archive::text_iarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   } \
   prefix1, prefix2 \
   void Type1, Type2::registerTextOArchive(boost::archive::text_oarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   } \
   prefix1, prefix2 \
   void Type1, Type2::registerXmlIArchive(boost::archive::xml_iarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   } \
   prefix1, prefix2 \
   void Type1, Type2::registerXmlOArchive(boost::archive::xml_oarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   } \
   prefix1, prefix2 \
   void Type1, Type2::registerBinaryIArchive(boost::archive::binary_iarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   } \
   prefix1, prefix2 \
   void Type1, Type2::registerBinaryOArchive(boost::archive::binary_oarchive &ar) { \
      ar.register_type<Type1, Type2 >(); \
   }

#define V_SERIALIZERS(Type) \
   void Type::registerTextIArchive(boost::archive::text_iarchive &ar) { \
      ar.register_type<Type >(); \
   } \
   void Type::registerTextOArchive(boost::archive::text_oarchive &ar) { \
      ar.register_type<Type >(); \
   } \
   void Type::registerXmlIArchive(boost::archive::xml_iarchive &ar) { \
      ar.register_type<Type >(); \
   } \
   void Type::registerXmlOArchive(boost::archive::xml_oarchive &ar) { \
      ar.register_type<Type >(); \
   } \
   void Type::registerBinaryIArchive(boost::archive::binary_iarchive &ar) { \
      ar.register_type<Type >(); \
   } \
   void Type::registerBinaryOArchive(boost::archive::binary_oarchive &ar) { \
      ar.register_type<Type >(); \
   }

#define V_OBJECT_TYPE3INT(Type, suffix, id) \
      class RegisterObjectType_##suffix { \
         public: \
                 RegisterObjectType_##suffix() { \
                    ObjectTypeRegistry::registerType<Type >(id); \
                    boost::serialization::void_cast_register<Type, Type::Base>( \
                          static_cast<Type *>(NULL), static_cast<Type::Base *>(NULL) \
                          ); \
                 } \
      }; \
      static RegisterObjectType_##suffix registerObjectType_##suffix; \

//! register a new Object type (complex form, specify suffix for symbol names)
#define V_OBJECT_TYPE3(Type, suffix, id) \
   namespace { \
      V_OBJECT_TYPE3INT(Type, suffix, id) \
   }

//! register a new Object type (complex form, specify suffix for symbol names)
#define V_OBJECT_TYPE4(Type1, Type2, suffix, id) \
   namespace { \
      namespace suffix { \
      typedef Type1,Type2 Type; \
      V_OBJECT_TYPE3INT(Type, suffix, id) \
   } \
   }

//! register a new Object type (simple form for non-templates, symbol suffix determined automatically)
#define V_OBJECT_TYPE(Type, id) \
   V_SERIALIZERS(Type) \
   V_OBJECT_TYPE3(Type, Type, id)

} // namespace vistle

#ifdef VISTLE_IMPL
#include "object_impl.h"
#endif
#endif
