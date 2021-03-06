#include <future>
#include <boost/algorithm/string/predicate.hpp>

// cover
#include <cover/coVRPluginSupport.h>
#include <cover/VRSceneGraph.h>
#include <cover/coVRAnimationManager.h>
#include <cover/coCommandLine.h>
#include <cover/OpenCOVER.h>
#include <cover/coVRPluginList.h>

// vistle
#include <module/renderer.h>
#include <util/exception.h>
#include <core/message.h>
#include <core/object.h>
#include <core/lines.h>
#include <core/triangles.h>
#include <core/polygons.h>
#include <core/texture1d.h>
#include <core/geometry.h>


#include <osg/Group>
#include <osg/Node>
#include <osg/Sequence>

#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>

#include "VistleRenderObject.h"
#include "VistleGeometryGenerator.h"
#include "VistleInteractor.h"

using namespace opencover;

using namespace vistle;

class OsgRenderer: public vistle::Renderer {

 public:
   OsgRenderer(const std::string &shmname,
         const std::string &name, int moduleId);
   ~OsgRenderer();

   bool compute();
   void render();

   void addInputObject(vistle::Object::const_ptr container,
         vistle::Object::const_ptr geometry,
         vistle::Object::const_ptr colors,
         vistle::Object::const_ptr normals,
         vistle::Object::const_ptr texture);
   bool addInputObject(const std::string & portName,
         vistle::Object::const_ptr object);

   osg::ref_ptr<osg::Group> vistleRoot;

   typedef std::map<std::string, VistleRenderObject *> ObjectMap;
   ObjectMap objects;
   bool removeObject(VistleRenderObject *ro);
   void removeAllCreatedBy(int creator);

   bool parameterAdded(const int senderId, const std::string &name, const message::AddParameter &msg, const std::string &moduleName);
   bool parameterChanged(const int senderId, const std::string &name, const message::SetParameter &msg);

   typedef std::map<int, VistleInteractor *> InteractorMap;
   InteractorMap m_interactorMap;

   struct DelayedObject {
      DelayedObject(VistleRenderObject *ro, VistleGeometryGenerator generator)
         : ro(ro)
         , generator(generator)
         , node_future(std::async(std::launch::async, generator))
      {}
      VistleRenderObject *ro;
      VistleGeometryGenerator generator;
      std::future<osg::Node *> node_future;
   };
   std::deque<DelayedObject> m_delayedObjects;

 protected:
   struct Creator {
      Creator(int id, const std::string &basename, osg::ref_ptr<osg::Group> parent)
      : id(id)
      , age(0)
      , interactor(nullptr)
      {
         std::stringstream s;
         s << basename << "_" << id;
         name = s.str();

         root = new osg::Group;
         root->setName(name);

         constant = new osg::Group;
         constant->setName(name + "_static");
         root->addChild(constant);

         animated = new osg::Sequence;
         animated->setName(name + "_animated");
         root->addChild(animated);

         VRSceneGraph::instance()->addNode(root, parent, NULL);
      }
      int id;
      int age;
      std::string name;
      VistleInteractor *interactor;
      osg::ref_ptr<osg::Group> root;
      osg::ref_ptr<osg::Group> constant;
      osg::ref_ptr<osg::Sequence> animated;
   };
   typedef std::map<int, Creator> CreatorMap;
   CreatorMap creatorMap;

};

OsgRenderer::OsgRenderer(const std::string &shmname,
      const std::string &name, int moduleId)
: vistle::Renderer("COVER", shmname, name, moduleId)
{
   vistle::registerTypes();
   createInputPort("data_in");

   vistleRoot = new osg::Group;
   vistleRoot->setName("VistlePlugin");
   VRSceneGraph::instance()->addNode(vistleRoot, (osg::Group*)NULL, NULL);

   setObjectReceivePolicy(message::ObjectReceivePolicy::Distribute);

   initDone();
}

OsgRenderer::~OsgRenderer() {

   for (std::map<std::string, VistleRenderObject *>::iterator it = objects.begin(), next=it;
         it != objects.end();
         it = next) {
      next = it;
      ++next;

      VistleRenderObject *ro = it->second;
      removeObject(ro);
   }

   VRSceneGraph::instance()->deleteNode("VistlePlugin", true);
}

bool OsgRenderer::parameterAdded(const int senderId, const std::string &name, const message::AddParameter &msg, const std::string &moduleName) {

   std::string plugin = moduleName;
   if (boost::algorithm::ends_with(name, "Old"))
      plugin = plugin.substr(0, plugin.size()-3);
   if (plugin == "CutGeometry")
      plugin = "CuttingSurface";

   if (plugin == "CuttingSurface"
         || plugin == "Tracer"
         || plugin == "IsoSurface") {
      cover->addPlugin(plugin.c_str());

#if 0
      auto creator = creatorMap.find(senderId);
      if (creator == creatorMap.end()) {
         creatorMap.insert(std::make_pair(senderId, Creator(senderId, moduleName, vistleRoot)));
      }
#endif

      InteractorMap::iterator it = m_interactorMap.find(senderId);
      if (it == m_interactorMap.end()) {
         m_interactorMap[senderId] = new VistleInteractor(this, moduleName, senderId);
         it = m_interactorMap.find(senderId);
      }
      VistleInteractor *inter = it->second;
      inter->addParam(name, msg);
   }
   return true;
}

bool OsgRenderer::parameterChanged(const int senderId, const std::string &name, const message::SetParameter &msg) {

   InteractorMap::iterator it = m_interactorMap.find(senderId);
   if (it == m_interactorMap.end()) {
      return false;
   }

   VistleInteractor *inter = it->second;
   inter->applyParam(name, msg);
   coVRPluginList::instance()->newInteractor(inter->getObject(), inter);

   return true;
}

bool OsgRenderer::removeObject(VistleRenderObject *ro) {
   if (!ro)
      return false;
   if (!ro->isGeometry()) {
      std::cerr << "Node is not geometry: " << ro->getName() << std::endl;
      return false;
   }

   std::string name = ro->getName();
   std::map<std::string, VistleRenderObject *>::iterator it = objects.find(name);
   if (it == objects.end()) {
      std::cerr << "Did not find node for " << name << std::endl;
      return false;
   }

   osg::Node *node = ro->node();
   if (node) {
      VRSceneGraph::instance()->deleteNode(ro->getName(), false);
      while (node->getNumParents() > 0) {
         osg::Group *parent = node->getParent(0);
         parent->removeChild(node);
      }
   }
   delete ro;
   objects.erase(it);

   return true;
}

bool OsgRenderer::compute() {

   return true;
}

void OsgRenderer::removeAllCreatedBy(int creator) {

   for (ObjectMap::iterator next, it = objects.begin();
         it != objects.end();
         it = next) {

      next = it;
      ++next;
      if (it->second->object()->getCreator() == creator) {
         removeObject(it->second);
      }
   }

   auto it = creatorMap.find(creator);
   if (it != creatorMap.end()) {
      Creator &cr = it->second;
      coVRAnimationManager::instance()->removeSequence(cr.animated);
      cr.animated->removeChildren(0, cr.animated->getNumChildren());
   }
}

void OsgRenderer::addInputObject(vistle::Object::const_ptr container,
                                 vistle::Object::const_ptr geometry,
                                 vistle::Object::const_ptr colors,
                                 vistle::Object::const_ptr normals,
                                 vistle::Object::const_ptr texture) {

   int creatorId = container->getCreator();
   CreatorMap::iterator it = creatorMap.find(creatorId);
   if (it != creatorMap.end()) {
      if (it->second.age < container->getExecutionCounter()) {
         std::cerr << "removing all created by " << creatorId << ", age " << container->getExecutionCounter() << ", was " << it->second.age << std::endl;
         removeAllCreatedBy(creatorId);
      } else if (it->second.age > container->getExecutionCounter()) {
         std::cerr << "received outdated object created by " << creatorId << ", age " << container->getExecutionCounter() << ", was " << it->second.age << std::endl;
         return;
      }
   } else {
      std::string name = getModuleName(container->getCreator());
      it = creatorMap.insert(std::make_pair(creatorId, Creator(container->getCreator(), name, vistleRoot))).first;
   }
   Creator &creator = it->second;
   creator.age = container->getExecutionCounter();

   if (objects.find(container->getName()) != objects.end()) {
      std::cerr << "Object added twice: " << container->getName() << std::endl;
      return;
   }

   VistleRenderObject *ro = new VistleRenderObject(container);
   ro->setGeometry(geometry);
   ro->setColors(colors);
   ro->setNormals(normals);
   ro->setTexture(texture);

   int t = geometry->getTimestep();
   if (t < 0 && colors) {
      t = colors->getTimestep();
   }
   if (t < 0 && normals) {
      t = normals->getTimestep();
   }
   if (t < 0 && texture) {
      t = texture->getTimestep();
   }
   ro->setTimestep(t);
   m_delayedObjects.push_back(DelayedObject(ro, VistleGeometryGenerator(geometry, colors, normals, texture)));
}


bool OsgRenderer::addInputObject(const std::string & portName,
                                 vistle::Object::const_ptr object) {

#if 0
   std::cout << "++++++OSGRenderer addInputObject " << object->getType()
             << " creator " << object->getCreator()
             << " exec " << object->getExecutionCounter()
             << " block " << object->getBlock()
             << " timestep " << object->getTimestep() << std::endl;
#endif

   switch (object->getType()) {
      case vistle::Object::GEOMETRY: {

         vistle::Geometry::const_ptr geom = vistle::Geometry::as(object);

#if 0
         std::cerr << "   Geometry: [ "
            << (geom->geometry()?"G":".")
            << (geom->colors()?"C":".")
            << (geom->normals()?"N":".")
            << (geom->texture()?"T":".")
            << " ]" << std::endl;
#endif
         addInputObject(object, geom->geometry(), geom->colors(), geom->normals(),
                        geom->texture());

         break;
      }

      case vistle::Object::PLACEHOLDER:
      default: {
         if (object->getType() == vistle::Object::PLACEHOLDER
               || VistleGeometryGenerator::isSupported(object->getType()))
            addInputObject(object, object, vistle::Object::ptr(), vistle::Object::ptr(), vistle::Object::ptr());

         break;
      }
   }

   return true;
}

void OsgRenderer::render() {

   if (m_delayedObjects.empty())
      return;

   int numReady = 0;
   for (size_t i=0; i<m_delayedObjects.size(); ++i) {
      auto &node_future = m_delayedObjects[i].node_future;
      if (!node_future.valid()) {
         std::cerr << "OsgRenderer::render(): future not valid" << std::endl;
         break;
      }
      auto status = node_future.wait_for(std::chrono::seconds(0));
#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ <= 6)
      if (!status)
         break;
#else
      if (status != std::future_status::ready)
         break;
#endif
      ++numReady;
   }

   int numAdd = 0;
   MPI_Allreduce(&numReady, &numAdd, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

   //std::cerr << "adding " << numAdd << " delayed objects, " << m_delayedObjects.size() << " waiting" << std::endl;
   for (int i=0; i<numAdd; ++i) {
      auto &node_future = m_delayedObjects.front().node_future;
      auto &ro = m_delayedObjects.front().ro;
      osg::Node *node = node_future.get();
      if (node) {
         int creatorId = ro->object()->getCreator();
         CreatorMap::iterator it = creatorMap.find(creatorId);
         assert(it != creatorMap.end());
         Creator &creator = it->second;

         ro->setNode(node);
         objects[ro->object()->getName()] = ro;
         node->setName(ro->object()->getName());
         osg::ref_ptr<osg::Group> parent = creator.constant;
         const int t = ro->timestep();
         if (t >= 0) {
            while (size_t(t) >= creator.animated->getNumChildren()) {
               creator.animated->addChild(new osg::Group);
            }
            parent = dynamic_cast<osg::Group *>(creator.animated->getChild(t));
            assert(parent);
            coVRAnimationManager::instance()->addSequence(creator.animated);
         }
         VRSceneGraph::instance()->addNode(ro->node(), parent, ro);
      } else {
         std::cerr << rank() << ": discarding delayed object " << ro->object()->getName() << ": no node created" << std::endl;
         delete ro;
      }
      m_delayedObjects.pop_front();
   }
}

class VistlePlugin: public opencover::coVRPlugin {

 public:
   VistlePlugin();
   ~VistlePlugin();
   bool init();
   void preFrame();
   void requestQuit(bool killSession);
   bool executeAll();

 private:
   OsgRenderer *m_module;
};

using opencover::coCommandLine;

VistlePlugin::VistlePlugin()
: m_module(nullptr)
{
   int initialized = 0;
   MPI_Initialized(&initialized);
   if (!initialized) {
      std::cerr << "VistlePlugin: no MPI support - started from within Vistle?" << std::endl;
      //throw(vistle::exception("no MPI support"));
      return;
   }

   if (coCommandLine::argc() < 3) {
      for (int i=0; i<coCommandLine::argc(); ++i) {
         std::cout << i << ": " << coCommandLine::argv(i) << std::endl;
      }
      throw(vistle::exception("at least 2 command line arguments required"));
   }

   int rank, size;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   const std::string &shmname = coCommandLine::argv(1);
   const std::string &name = coCommandLine::argv(2);
   int moduleID = atoi(coCommandLine::argv(3));

   m_module = new OsgRenderer(shmname, name, moduleID);
}

VistlePlugin::~VistlePlugin() {

   if (m_module) {
      MPI_Barrier(MPI_COMM_WORLD);
      delete m_module;
   }
}

bool VistlePlugin::init() {

   return m_module;
}

void VistlePlugin::preFrame() {

   MPI_Barrier(MPI_COMM_WORLD);
   if (m_module && !m_module->dispatch()) {
      std::cerr << "Vistle requested COVER to quit" << std::endl;
      OpenCOVER::instance()->quitCallback(NULL,NULL);
   }
}

void VistlePlugin::requestQuit(bool killSession)
{
   delete m_module;
   m_module = nullptr;
}

bool VistlePlugin::executeAll() {

   message::Execute exec; // execute all sources in data flow graph
   exec.setDestId(message::Id::MasterHub);
   m_module->sendMessage(exec);
   return true;
}

COVERPLUGIN(VistlePlugin);
