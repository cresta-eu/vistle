#ifndef OBJECTCACHE_H
#define OBJECTCACHE_H

#include <string>
#include <map>
#include "object.h"

namespace vistle {

typedef std::list<vistle::Object::const_ptr> ObjectList;

class V_COREEXPORT ObjectCache {

   public:
      enum CacheMode {
         CacheDefault = -1,
         CacheNone,
         CacheAll
      };

      ObjectCache();
      ~ObjectCache();

      void clear();
      CacheMode cacheMode() const;
      void setCacheMode(CacheMode mode);

      void addObject(const std::string &portname, Object::const_ptr object);
      const ObjectList &getObjects(const std::string &portname) const;

   private:
      CacheMode m_cacheMode;
      std::map<std::string, ObjectList> m_cache;
      ObjectList m_emptyList;
};

} // namespace vistle
#endif

