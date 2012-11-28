#ifndef _DOFMANAGER_H_
#define _DOFMANAGER_H_

#include <string>
#include <map>

#include "Dof.h"
#include "Comparators.h"
#include "FunctionSpace.h"

/**
   @class DofManager
   @brief This class manages the Degrees Of Freedom (Dof)
   
   This class numbers the Degrees Of Freedom (Dof).@n

   It can map a Dof to a @em global @c ID.@n
   Those @c IDs are handeld by the DofManager itself.@n

   Finaly, this class allows to fix a Dof to a given value 
   (in that case, we have a fixed Dof).@n
   Note that we call an @em unknown, a Dof that is @em not fixed.

   @warning
   Up to know, a mapped Dof @em can't be @em deleted.@n
*/

class DofManager{
 private:
  std::map<const Dof*, int, DofComparator>*    globalId;
  std::map<const Dof*, double, DofComparator>* fixedDof;

  int nextId;
  
 public:
   DofManager(void);
  ~DofManager(void);

  void addToGlobalIdSpace(const std::vector<GroupOfDof*>& god);
  int  getGlobalId(const Dof& dof) const;

  bool isUnknown(const Dof& dof) const;
  bool fixValue(const Dof& dof, double value);
  std::pair<bool, double> getValue(const Dof& dof) const;

  unsigned int getDofNumber(void) const;
  unsigned int getUnkownNumber(void) const;  

  std::string toString(void) const;
};


/**
   @fn DofManager::DofManager
   
   Instantiates a new DofManager
   **

   @fn DofManager::~DofManager
   
   Deletes this DofManager
   **

   @fn DofManager::addToGlobalIdSpace
   @param god A vector of GroupOfDof

   Numbers every Dof of every given GroupOfDof.@n

   Each Dof%s will be given a @em unique @em global @c ID .
   **

   @fn DofManager::getGlobalId
   @param dof The Dof from which we want the @em global @c ID
   @return Returns the @em global @em @c ID of the given Dof
   **

   @fn DofManager::isUnknown
   @param dof A Dof
   @return Returns:
   @li @c true, if the given Dof is an unknwon
   (@em i.e. a non fixed Dof)
   @li @c false, otherwise
   **
   
   @fn DofManager::fixValue
   @param dof A Dof
   @param value A real number
   
   Fixes the given Dof to the given value

   @return Returns:
   @li @c true, if the operation is a success
   @li @c false, otherwise
   
   @note
   Here are two important cases, where fixValue() will fail:
   @li The given Dof is not in this DofManager
   @li The given Dof is already fixed
   **

   @fn DofManager::getValue
   @param dof A Dof
   @return Returns an std::pair, where:
   <ul>
     <li> The first value is:
       <ol>
         <li> @c true, if the given Dof @em is a @em fixed Dof
         <li> @c false, otherwise
       </ol>
   
     <li> The second value is:
       <ol>
         <li> If the first value was @em @c true, 
	 equal the value of the given (fixed) Dof
         <li> Not specified otherwise
       </ol>
   </ul>
   **

   @fn DofManager::getDofNumber
   @return Returns the number of Dof%s in 
   this DofManager (with fixed Dof%s)
   **

   @fn DofManager::getUnkownNumber  
   @return Returns the number of fixed Dof%s
   in this DofManager
   **

   @fn  DofManager::toString
   @return Returns the DofManager's string
   **
*/

//////////////////////
// Inline Functions //
//////////////////////

inline bool DofManager::isUnknown(const Dof& dof) const{
  return fixedDof->count(&dof) == 0;
}

inline unsigned int DofManager::getDofNumber(void) const{
  return globalId->size() - fixedDof->size();
}

inline unsigned int DofManager::getUnkownNumber(void) const{
  return fixedDof->size();
}

#endif
