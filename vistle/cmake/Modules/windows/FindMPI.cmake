# - Find MPI
# Find the MPI includes and library
#
#  MPI_INCLUDE_DIR - Where to find MPI includes
#  MPI_LIBRARIES   - List of libraries when using MPI
#  MPI_FOUND       - True if MPI was found

IF(MPI_INCLUDE_DIR)
  SET(MPI_FIND_QUIETLY TRUE)
ENDIF(MPI_INCLUDE_DIR)

FIND_PATH(MPI_INCLUDE_DIR "MPI.h"
  PATHS
  $ENV{MPI_HOME}/include
  $ENV{EXTERNLIBS}/OpenMPI/include
  $ENV{EXTERNLIBS}/MPICH/include
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
  DOC "MPI - Headers"
)
set(MPI_C_INCLUDE_PATH ${MPI_INCLUDE_DIR})

SET(MPIDIRS $ENV{MPI_HOME}
  $ENV{EXTERNLIBS}/OpenMPI
  $ENV{EXTERNLIBS}/MPICH
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw
  /opt/local
  /opt/csw
  /opt
  )

FIND_LIBRARY(MPI_LIBRARY NAMES libmpi
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPI - Library"
)
FIND_LIBRARY(MPI_CXX_LIBRARY NAMES libmpi_cxx
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)
FIND_LIBRARY(MPI_PAL_LIBRARY NAMES libopen-pal
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)
FIND_LIBRARY(MPI_RTE_LIBRARY NAMES libopen-rte
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)
FIND_LIBRARY(MPI_LIBRARY_DEBUG NAMES libmpid
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPI - Library"
)
FIND_LIBRARY(MPI_CXX_LIBRARY_DEBUG NAMES libmpi_cxxd
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)
FIND_LIBRARY(MPI_PAL_LIBRARY_DEBUG NAMES libopen-pald
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)
FIND_LIBRARY(MPI_RTE_LIBRARY_DEBUG NAMES libopen-rted
  PATHS ${MPIDIRS}
  PATH_SUFFIXES lib lib64
  DOC "MPICXX - Library"
)


INCLUDE(FindPackageHandleStandardArgs)

IF(MSVC)
  
  IF(MPI_LIBRARY_DEBUG AND MPI_LIBRARY)
    SET(MPI_LIBRARIES optimized ${MPI_LIBRARY} debug ${MPI_LIBRARY_DEBUG} optimized ${MPI_PAL_LIBRARY} debug ${MPI_PAL_LIBRARY_DEBUG} optimized ${MPI_RTE_LIBRARY} debug ${MPI_RTE_LIBRARY_DEBUG})
    SET(MPI_C_LIBRARIES optimized ${MPI_LIBRARY} debug ${MPI_LIBRARY_DEBUG} optimized ${MPI_PAL_LIBRARY} debug ${MPI_PAL_LIBRARY_DEBUG} optimized ${MPI_RTE_LIBRARY} debug ${MPI_RTE_LIBRARY_DEBUG})
    SET(_CXX optimized ${MPI_CXX_LIBRARY} debug ${MPI_CXX_LIBRARY_DEBUG} optimized ${MPI_LIBRARY} debug ${MPI_LIBRARY_DEBUG} optimized ${MPI_PAL_LIBRARY} debug ${MPI_PAL_LIBRARY_DEBUG} optimized ${MPI_RTE_LIBRARY} debug ${MPI_RTE_LIBRARY_DEBUG})
  ENDIF(MPI_LIBRARY_DEBUG AND MPI_LIBRARY)

  FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPI DEFAULT_MSG MPI_LIBRARY MPI_LIBRARY_DEBUG MPI_INCLUDE_DIR)

  MARK_AS_ADVANCED(MPI_LIBRARY MPI_LIBRARY_DEBUG MPI_CXX_LIBRARY MPI_CXX_LIBRARY_DEBUG MPI_PAL_LIBRARY MPI_PAL_LIBRARY_DEBUG MPI_RTE_LIBRARY MPI_RTE_LIBRARY_DEBUG MPI_INCLUDE_DIR)
  
ELSE(MSVC)
  # rest of the world
  SET(MPI_LIBRARIES ${MPI_LIBRARY})

  FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPI DEFAULT_MSG MPI_LIBRARY MPI_INCLUDE_DIR)
  
  MARK_AS_ADVANCED(MPI_LIBRARY MPI_CXX_LIBRARY MPI_LIBRARY_DEBUG MPI_PAL_LIBRARY MPI_RTE_LIBRARY  MPI_INCLUDE_DIR)
  
ENDIF(MSVC)

IF(MPI_FOUND)
  SET(MPI_INCLUDE_DIRS ${MPI_INCLUDE_DIR})
ENDIF(MPI_FOUND)