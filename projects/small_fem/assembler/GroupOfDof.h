#ifndef _GROUPOFDOF_H_
#define _GROUPOFDOF_H_

#include <vector>

#include "Dof.h"
#include "MElement.h"

/**
   @class GroupOfDof
   @brief Handels a Group of Dof%s with @em geometrical meaning

   This class handles a collection of Dof%s with a @em geometrical meaning 
   (@e e.g: Dof%s that belongs to the same (finite) element).@n

   It also gives acces to the underlying Geometrical Element.
*/

class GroupOfDof{
 private:
  const MElement* element;

  unsigned int nDof;
  std::vector<const Dof*>* dof;
  
  unsigned int nextDof;

 public:
   GroupOfDof(unsigned int numberOfDof, 
	      const MElement& geoElement);

  ~GroupOfDof(void);

  void add(const Dof& dof);
  
  unsigned int                   getNumber(void)     const;
  const Dof&                     get(unsigned int i) const; 
  const std::vector<const Dof*>& getAll(void)        const;
  
  const MElement& getGeoElement(void) const;

  std::string toString(void) const;
};


/**
   @fn GroupOfDof::GroupOfDof
   @param numberOfDof A natural number
   @param geoElement A geomtrical Element (MElement)

   Instanciates a new GroupOfDof related to the given
   Element and that can contains @c numberOfDof Dof%s
   **

   @fn GroupOfDof::~GroupOfDof

   Deletes this GroupOfDof
   **

   @fn GroupOfDof::add
   @param dof
 
   Adds the given Dof to this GroupOfDof
   **

   @fn GroupOfDof::getNumber
   @return Returns the number of elements in this GroupOfDof
   **

   @fn GroupOfDof::get
   @param i An interger ranging from 0 
   to GroupOfDof::getNumber() - 1
   @return Returns the ith element of the Group
   **

   @fn GroupOfDof::getAll
   @return Returns all the elements of the Group
   **

   @fn GroupOfDof::getGeoElement
   @return Returns the underlying Geometrical Element
   **

   @fn GroupOfDof::toString
   @return Returns a string discribing this GroupOfDof
*/

//////////////////////
// Inline Functions //
//////////////////////

inline unsigned int GroupOfDof::getNumber(void) const{
  return nDof;
}

inline const Dof& GroupOfDof::get(unsigned int i) const{
  return *((*dof)[i]);
}
 
inline const std::vector<const Dof*>& GroupOfDof::getAll(void) const{
  return *dof;
}

inline const MElement& GroupOfDof::getGeoElement(void) const{
  return *element;
}

#endif
