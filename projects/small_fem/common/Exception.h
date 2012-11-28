#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <string>
#include <exception>
#include <cstdarg>

/**
   @class Exception
   @brief A general class for exceptions

   Exception is a general class for exceptions.@n

   An Exception is instantiated with a @em string.@n
   This string is used as a @em description 
   of the @em cause of the exception.
 */

class Exception: public std::exception{
 protected:
  std::string* why;
  
 public:
  Exception(const std::string format, ...);

  virtual ~Exception(void) throw();
  virtual const char* what(void) const throw();
};

/**
   @fn Exception::Exception
   @param format A @em formated string 
   of the @em cause of the exception
   @param ... The parameters of the formated string
   
   Instantiates a new Exception
   **

   @fn Exception::~Exception
   Deletes this Exception
   **

   @fn const char* Exception::what
   @return Returns the @em cause of the exception
 */

#endif
