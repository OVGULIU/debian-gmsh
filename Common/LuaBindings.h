// Gmsh - Copyright (C) 1997-2010 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.
//
// Contributor(s):
//   Jonathan Lambrechts
//

#ifndef _LUA_BINDINGS_H_
#define _LUA_BINDINGS_H_

#include <map>
#include <vector>
#include <list>
#include <set>
#include "GmshConfig.h"
#include "GmshMessage.h"
#include "Bindings.h"
#include "SVector3.h"

#if defined(HAVE_LUA)

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// store a unique static class name for each binded class
template <typename type>
class className{
  static std::string _name;
 public:
  static void set(std::string name)
  {
    if(_name != "") Msg::Error("Class name already set");
    _name = name;
  }
  static const std::string &get()
  {
    if(_name == "") Msg::Error("Class unknown to Lua");
    return _name;
  }
};

template<typename type>
std::string  className<type>::_name;

template <typename t>
class className<t*>{
 public:
  static const std::string get(){ return className<t>::get(); }
};

template <typename t>
class className<const t &>{
 public:
  static const std::string get(){ return className<t>::get(); }
};

class classBinding;
class binding {
  static binding *_instance;
  void checkDocCompleteness();
 public:
  static void *checkudata_with_inheritance (lua_State *L, int ud, const char *tname);
  inline static binding *instance(){ return _instance ? _instance : new binding(); }
  lua_State *L;
  int readFile(const char *filename);
  void interactiveSession();
  binding();
  ~binding();
  std::map<std::string,classBinding *> classes;
  template<class t>
  classBinding *addClass(std::string className);
};

template <>
class className<lua_State>{
 public:
  static const std::string get(){ return "-1"; }
};

// lua Stack: templates to get/push value from/on the lua stack

template<class type>
class luaStack {
};

template <>
class luaStack<void>
{
 public:
  static std::string getName(){ return "void"; }
};

template<>
class luaStack<lua_State *>{
 public:
  static lua_State *get(lua_State *L, int ia){ return L; }
  static std::string getName(){ return "-1"; }
};

template<>
class luaStack<int>{
 public:
  static int get(lua_State *L, int ia){ return luaL_checkint(L, ia); }
  static void push(lua_State *L, int i){ lua_pushinteger(L, i); }
  static std::string getName(){ return "int"; }
};

template<>
class luaStack<bool>{
 public:
  static int get(lua_State *L, int ia){ return lua_toboolean(L, ia); }
  static void push(lua_State *L, bool i){ lua_pushboolean(L, i); }
  static std::string getName(){ return "bool"; }
};
template<>
class luaStack<unsigned int>{
 public:
  static int get(lua_State *L, unsigned int ia)
  { 
    return (unsigned int)luaL_checkint(L, ia);
  }
  static void push(lua_State *L, unsigned int i){ lua_pushinteger(L, i); }
  static std::string getName(){ return "unsigned int"; }
};

template<class type>
class luaStack<std::vector<type > >{
 public:
  static std::vector<type> get(lua_State *L, int ia)
  {
    std::vector<type> v;
    size_t size = lua_objlen(L, ia);
    v.resize(size);
    for(size_t i = 0; i < size; i++){
      lua_rawgeti(L, ia, i + 1);
      v[i] = luaStack<type>::get(L, -1);
      lua_pop(L, 1);
    }
    return v;
  }
  static void push(lua_State *L, const std::vector<type>& v)
  {
    lua_createtable(L, v.size(), 0);
    for(size_t i = 0; i < v.size(); i++){
      luaStack<type>::push(L, v[i]);
      lua_rawseti(L, 2, i + 1);
    }
  }
  static std::string getName()
  {
    std::string name = "vector of ";
    return name + luaStack<type>::getName();
  }
};
template<class lType>
class luaStack<std::list<lType > >{
 public:
  static std::list<lType> get(lua_State *L, int ia)
  {
    std::list<lType> l;
    size_t size = lua_objlen(L, ia);
    for(size_t i = 0; i < size; i++){
      lua_rawgeti(L, ia, i + 1);
      l.push_back(luaStack<lType>::get(L, -1));
      lua_pop(L, 1);
    }
    return l;
  }
  static void push(lua_State *L, const std::list<lType>& l)
  {
    std::vector<lType> v(l.begin(), l.end());
    lua_createtable(L, v.size(), 0);
    for(size_t i = 0; i < v.size(); i++){
      luaStack<lType>::push(L, v[i]);
      lua_rawseti(L, 2, i + 1);
    }
  }
  static std::string getName()
  {
    std::string name = "list of ";
    return name + luaStack<lType>::getName();
  }
};


template<class type>
class luaStack<std::vector<type> &>{
 public:
  static std::vector<type> get(lua_State *L, int ia)
  {
    std::vector<type> v;
    size_t size = lua_objlen(L, ia);
    v.resize(size);
    for(size_t i = 0; i< size; i++){
      lua_rawgeti(L, ia, i + 1);
      v[i]=luaStack<type>::get(L, -1);
      lua_pop(L, 1);
    }
    return v;
  }
  static void push(lua_State *L, const std::vector<type>& v)
  {
    lua_createtable(L, v.size(), 0);
    for(size_t i = 0; i < v.size; i++){
      luaStack<type>::push(L, v[i]);
      lua_rawseti(L, 2, i + 1);
    }
  }
  static std::string getName()
  {
    std::string name="vector of ";
    return name + luaStack<type>::getName();
  }
};
template<class lType>
class luaStack<std::list<lType > &>{
 public:
  static std::list<lType> get(lua_State *L, int ia)
  {
    std::list<lType> l;
    size_t size = lua_objlen(L, ia);
    for(size_t i = 0; i < size; i++){
      lua_rawgeti(L, ia, i + 1);
      l.push_back(luaStack<lType>::get(L, -1));
      lua_pop(L, 1);
    }
    return l;
  }
  static void push(lua_State *L, const std::list<lType>& l)
  {
    std::vector<lType> v(l.begin(), l.end());
    lua_createtable(L, v.size(), 0);
    for(size_t i = 0; i < v.size(); i++){
      luaStack<lType>::push(L, v[i]);
      lua_rawseti(L, 2, i + 1);
    }
  }
  static std::string getName()
  {
    std::string name = "list of ";
    return name + luaStack<lType>::getName();
  }
};

template<>
class luaStack<double>{
 public:
  static double get(lua_State *L, int ia){ return luaL_checknumber(L, ia); }
  static void push(lua_State *L, double v){ lua_pushnumber(L, v); }
  static std::string getName(){ return "double"; }
};

template <>
class luaStack<SVector3>{
 public:
  static SVector3 get(lua_State *L, int ia)
  {
    double v[3];
    size_t size = lua_objlen(L, ia);
    if (size!=3)
      luaL_typerror(L, ia, "SVector3");
    for(size_t i = 0; i< size; i++){
      lua_rawgeti(L, ia, i + 1);
      v[i] = luaStack<double>::get(L, -1);
      lua_pop(L, 1);
    }
    return SVector3(v[0],v[1],v[2]);
  }
  static void push(lua_State *L, const SVector3& v)
  {
    lua_createtable(L, 3, 0);
    for(size_t i = 0; i < 3; i++){
      luaStack<double>::push(L, v[i]);
      lua_rawseti(L, 2, i + 1);
    }
  }
  static std::string getName()
  {
    return "3-uple of double";
  }
};

template<>
class luaStack<std::string>{
  public:
  static std::string get(lua_State *L, int ia){ return luaL_checkstring(L, ia); }
  static void push(lua_State *L, std::string s){ lua_pushstring(L, s.c_str()); }
  static std::string getName(){ return "string"; }
};

template <typename type>
class luaStack<type *>{
  typedef struct { type *pT; } userdataType;
  public:
  static type* get(lua_State *L, int ia)
  {
    userdataType *ud = static_cast<userdataType*>
      (binding::checkudata_with_inheritance (L, ia, getName().c_str()));
    if(!ud) luaL_typerror(L, ia, className<type>::get().c_str());
    return ud->pT;
  }
  static void push(lua_State *L, type *obj)
  {
    userdataType *ud = static_cast<userdataType*>
      (lua_newuserdata(L, sizeof(userdataType)));
    ud->pT=obj;
    // lookup metatable in Lua registry
    luaL_getmetatable(L,className<type>::get().c_str());
    lua_setmetatable(L, -2);
  }
  static std::string getName()
  {
    return className<type>::get();
  }
};

template <typename type>
class luaStack<const type *>{
  typedef struct { type *pT; } userdataType;
  public:
  static type* get(lua_State *L, int ia)
  {
    userdataType *ud = static_cast<userdataType*>
      (binding::checkudata_with_inheritance (L, ia, getName().c_str()));
    if(!ud) luaL_typerror(L, ia, className<type>::get().c_str());
    return ud->pT;
  }
  static void push(lua_State *L, const type *obj)
  {
    userdataType *ud = static_cast<userdataType*>
      (lua_newuserdata(L, sizeof(userdataType)));
    ud->pT=(type*)obj;
    // lookup metatable in Lua registry
    luaL_getmetatable(L,className<type>::get().c_str());
    lua_setmetatable(L, -2);
  }
  static std::string getName()
  {
    return className<type>::get();
  }
};

template <typename type>
class luaStack<type &>{
  typedef struct { type *pT; } userdataType;
 public:
  static type& get(lua_State *L, int ia)
  {
    userdataType *ud = static_cast<userdataType*>
      (binding::checkudata_with_inheritance (L, ia, getName().c_str()));
    if(!ud) luaL_typerror(L, ia, className<type>::get().c_str());
    return *ud->pT; 
  }
  static std::string getName()
  {
    return className<type>::get();
  }
};

template <typename type>
class luaStack<const type &>{
  typedef struct { type *pT; } userdataType;
 public:
  static type& get(lua_State *L, int ia)
  {
    userdataType *ud = static_cast<userdataType*>
      (binding::checkudata_with_inheritance (L, ia, getName().c_str()));
    if(!ud) luaL_typerror(L, ia, className<type>::get().c_str());
    return *ud->pT; 
  }
  static std::string getName()
  {
    return className<type>::get();
  }
};

//static
template <typename cb>
class argTypeNames;

template <typename tr, typename t0, typename t1, typename t2, typename t3, 
  typename t4, typename t5, typename t6, typename t7, typename t8>
  class argTypeNames<tr (*)(t0, t1, t2, t3, t4, t5, t6, t7, t8)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
    names.push_back(luaStack<t4>::getName());
    names.push_back(luaStack<t5>::getName());
    names.push_back(luaStack<t6>::getName());
    names.push_back(luaStack<t7>::getName());
    names.push_back(luaStack<t8>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2, typename t3, 
          typename t4, typename t5, typename t6, typename t7>
  class argTypeNames<tr (*)(t0, t1, t2, t3, t4, t5, t6, t7)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
    names.push_back(luaStack<t4>::getName());
    names.push_back(luaStack<t5>::getName());
    names.push_back(luaStack<t6>::getName());
    names.push_back(luaStack<t7>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2, typename t3, 
          typename t4, typename t5, typename t6>
  class argTypeNames<tr (*)(t0, t1, t2, t3, t4, t5, t6)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
    names.push_back(luaStack<t4>::getName());
    names.push_back(luaStack<t5>::getName());
    names.push_back(luaStack<t6>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2, typename t3, 
          typename t4, typename t5>
class argTypeNames<tr (*)(t0, t1, t2, t3, t4, t5)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
    names.push_back(luaStack<t4>::getName());
    names.push_back(luaStack<t5>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2, typename t3, 
          typename t4>
class argTypeNames<tr (*)(t0, t1, t2, t3, t4)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
    names.push_back(luaStack<t4>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2, typename t3>
class argTypeNames<tr (*)(t0, t1, t2, t3)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
    names.push_back(luaStack<t3>::getName());
  }
};

template <typename tr, typename t0, typename t1, typename t2>
class argTypeNames<tr (*)(t0, t1, t2)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
    names.push_back(luaStack<t2>::getName());
  }
};

template <typename tr, typename t0, typename t1>
class argTypeNames<tr (*)(t0, t1)>{
  public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
    names.push_back(luaStack<t1>::getName());
  }
};

template <typename tr, typename t0>
class argTypeNames<tr (*)(t0)>{
  public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
    names.push_back(luaStack<t0>::getName());
  }
};

template <typename tr>
class argTypeNames<tr (*)()>{
  public:
  static void get(std::vector<std::string> &names)
  {
    names.clear();
    names.push_back(luaStack<tr>::getName());
  }
};

template <typename cb>
class argTypeNames;

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
  typename t3, typename t4, typename t5, typename t6, typename t7, typename t8>
  class argTypeNames<tr (tObj::*)(t0, t1, t2, t3, t4, t5, t6, t7, t8)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3, t4, t5, t6, t7, t8)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3, typename t4, typename t5, typename t6, typename t7>
  class argTypeNames<tr (tObj::*)(t0, t1, t2, t3, t4, t5, t6, t7)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3, t4, t5, t6, t7)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3, typename t4, typename t5, typename t6>
  class argTypeNames<tr (tObj::*)(t0, t1, t2, t3, t4, t5, t6)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3, t4, t5, t6)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3, typename t4, typename t5>
class argTypeNames<tr (tObj::*)(t0, t1, t2, t3, t4, t5)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3, t4, t5)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3, typename t4>
class argTypeNames<tr (tObj::*)(t0, t1, t2, t3, t4)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3, t4)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3>
class argTypeNames<tr (tObj::*)(t0, t1, t2, t3)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2, t3)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2>
class argTypeNames<tr (tObj::*)(t0, t1, t2)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1, t2)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1>
class argTypeNames<tr (tObj::*)(t0, t1)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0, t1)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0>
class argTypeNames<tr (tObj::*)(t0)>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)(t0)>::get(names);
  }
};

template <typename tr, typename tObj>
class argTypeNames<tr (tObj::*)()>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr(*)()>::get(names);
  }
};

// const 
template <typename cb>
class argTypeNames;
template <typename tr, typename tObj, typename t0, typename t1, typename t2,
          typename t3>
class argTypeNames<tr (tObj::*)(t0, t1, t2, t3)const>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr (*)(t0, t1, t2, t3)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1, typename t2>
class argTypeNames<tr (tObj::*)(t0, t1, t2)const>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr (*)(t0, t1, t2)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0, typename t1>
class argTypeNames<tr (tObj::*)(t0, t1)const>{
  public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr (*)(t0, t1)>::get(names);
  }
};

template <typename tr, typename tObj, typename t0>
class argTypeNames<tr (tObj::*)(t0)const>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr (*)(t0)>::get(names);
  }
};

template <typename tr, typename tObj>
class argTypeNames<tr (tObj::*)()const>{
 public:
  static void get(std::vector<std::string> &names)
  {
    argTypeNames<tr (*)()>::get(names);
  }
};

// template to call c function from the lua stack
// static, return 
template < typename tRet, typename t0, typename t1, typename t2, typename t3, typename t4, typename t5, typename t6, typename t7, typename t8>
  static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3, t4, t5, t6, t7, t8))
{
  if (lua_gettop(L) == 10) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
				luaStack<t4>::get(L, 5), luaStack<t5>::get(L, 6),
				luaStack<t6>::get(L, 7), luaStack<t7>::get(L, 8),
				luaStack<t8>::get(L, 9)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2, typename t3, typename t4, typename t5, typename t6, typename t7>
  static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3, t4, t5, t6, t7))
{
  if (lua_gettop(L) == 9) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
				luaStack<t4>::get(L, 5), luaStack<t5>::get(L, 6),
				luaStack<t6>::get(L, 7), luaStack<t7>::get(L, 8)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2, typename t3, typename t4, typename t5, typename t6>
  static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3, t4, t5, t6))
{
  if (lua_gettop(L) == 8) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
				luaStack<t4>::get(L, 5), luaStack<t5>::get(L, 6),
				luaStack<t6>::get(L, 7)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2, typename t3, typename t4, typename t5>
  static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3, t4, t5))
{
  if (lua_gettop(L) == 7) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
				luaStack<t4>::get(L, 5), luaStack<t5>::get(L, 6)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2, typename t3, typename t4>
  static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3,t4))
{
  if (lua_gettop(L) == 6) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
				luaStack<t4>::get(L, 5)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2, typename t3>
static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2, t3))
{
  if (lua_gettop(L) == 5) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4)));
  return 1;
};

template < typename tRet, typename t0, typename t1, typename t2>
static int luaCall(lua_State *L, tRet (*_f)(t0, t1, t2))
{
  if (lua_gettop(L) == 4) lua_remove(L,1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                luaStack<t2>::get(L, 3)));
  return 1;
};

template < typename tRet, typename t0, typename t1>
static int luaCall(lua_State *L, tRet (*_f)(t0, t1))
{
  if (lua_gettop(L) == 3) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2)));
  return 1;
};

template < typename tRet, typename t0>
static int luaCall(lua_State *L, tRet (*_f)(t0))
{
  if (lua_gettop(L) == 2) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)(luaStack<t0>::get(L, 1)));
  return 1;
};

template < typename tRet>
static int luaCall(lua_State *L, tRet (*_f)())
{
  if (lua_gettop(L) == 1) lua_remove(L, 1);
  luaStack<tRet>::push(L, (*_f)());
  return 1;
};

//static, no return 
template < typename t0, typename t1, typename t2, typename t3, typename t4, typename t5, typename t6>
  static int luaCall(lua_State *L, void (*_f)(t0, t1, t2, t3, t4, t5, t6))
{
  if (lua_gettop(L) == 8) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
          luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4), luaStack<t4>::get(L, 5),
	  luaStack<t5>::get(L, 6), luaStack<t6>::get(L, 7));
  return 1;
};

template < typename t0, typename t1, typename t2, typename t3, typename t4, typename t5>
  static int luaCall(lua_State *L, void (*_f)(t0, t1, t2, t3, t4, t5))
{
  if (lua_gettop(L) == 7) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
          luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4), luaStack<t4>::get(L, 5),
	  luaStack<t5>::get(L, 6));
  return 1;
};

template < typename t0, typename t1, typename t2, typename t3, typename t4>
  static int luaCall(lua_State *L, void (*_f)(t0, t1, t2, t3, t4))
{
  if (lua_gettop(L) == 6) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
          luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4), luaStack<t4>::get(L, 5));
  return 1;
};

template < typename t0, typename t1, typename t2, typename t3>
static int luaCall(lua_State *L, void (*_f)(t0, t1, t2, t3))
{
  if (lua_gettop(L) == 5) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
          luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4));
  return 1;
};

template <typename t0, typename t1, typename t2>
static int luaCall(lua_State *L, void (*_f)(t0, t1, t2))
{
  if (lua_gettop(L) == 4) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),luaStack<t2>::get(L, 3));
  return 1;
};

template <typename t0, typename t1>
static int luaCall(lua_State *L,void (*_f)(t0, t1))
{
  if (lua_gettop(L) == 3) lua_remove(L,1);
  (*(_f))(luaStack<t0>::get(L,1),luaStack<t1>::get(L,2));
  return 1;
};

template <typename t0>
static int luaCall(lua_State *L, void (*_f)(t0))
{
  if (lua_gettop(L) == 2) lua_remove(L, 1);
  (*(_f))(luaStack<t0>::get(L, 1));
  return 1;
};

template <>
#if (__GNUC__< 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3))
static 
#endif
int luaCall<void>(lua_State *L,void (*_f)())
{
  if (lua_gettop(L) == 1) lua_remove(L,1);
  (*(_f))();
  return 1;
}

//const, return
template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
  typename t3, typename t4, typename t5, typename t6, typename t7>
  static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5, t6, t7) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6),
			luaStack<t5>::get(L, 7), luaStack<t6>::get(L, 8), luaStack<t7>::get(L, 9)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
  typename t3, typename t4, typename t5, typename t6>
  static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5, t6) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6),
			luaStack<t5>::get(L, 7), luaStack<t6>::get(L, 8)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
  typename t3, typename t4, typename t5>
  static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6),
			luaStack<t5>::get(L, 7)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
  typename t3, typename t4>
  static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3, t4) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
          typename t3>
static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2>
static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2) const)
{
  luaStack<tRet>::push(L,(luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1>
static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2),luaStack<t1>::get(L, 3)));
  return 1;
};

template <typename tObj, typename tRet, typename t0>
static int luaCall(lua_State *L, tRet (tObj::*_f)(t0) const)
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2)));
  return 1;
};

template <typename tObj, typename tRet>
static int luaCall(lua_State *L, tRet (tObj::*_f)() const) {
  luaStack<tRet>::push(L,(luaStack<tObj*>::get(L,1)->*(_f))());
  return 1;
};

//non const, return
template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
          typename t3, typename  t4, typename t5, typename t6, typename t7>
  static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5, t6, t7))
{
  luaStack<tRet>::push(L,(luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5),
                        luaStack<t4>::get(L, 6), luaStack<t5>::get(L, 7), 
			luaStack<t6>::get(L, 8), luaStack<t7>::get(L, 9)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
          typename t3, typename  t4, typename t5, typename t6>
  static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5, t6))
{
  luaStack<tRet>::push(L,(luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5),
                        luaStack<t4>::get(L, 6), luaStack<t5>::get(L, 7), 
			luaStack<t6>::get(L, 8)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2,
          typename t3, typename  t4, typename t5>
static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1, t2, t3, t4, t5))
{
  luaStack<tRet>::push(L,(luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5),
                        luaStack<t4>::get(L, 6), luaStack<t5>::get(L, 7)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2, 
          typename t3, typename t4>
static int luaCall(lua_State *L, tRet (tObj::*_f)(t0, t1, t2, t3, t4))
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), 
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5),
                        luaStack<t4>::get(L, 6)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2, 
          typename t3>
static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1, t2, t3)) {
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L,1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4), luaStack<t3>::get(L, 5)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1, typename t2>
static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1, t2)) {
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3),
                        luaStack<t2>::get(L, 4)));
  return 1;
};

template <typename tObj, typename tRet, typename t0, typename t1>
static int luaCall(lua_State *L,tRet (tObj::*_f)(t0, t1))
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L, 1)->*(_f))
                       (luaStack<t0>::get(L, 2),luaStack<t1>::get(L, 3)));
  return 1;
};

template <typename tObj, typename tRet, typename t0>
static int luaCall(lua_State *L,tRet (tObj::*_f)(t0))
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L,1)->*(_f))(luaStack<t0>::get(L, 2)));
  return 1;
};

template <typename tObj, typename tRet>
static int luaCall(lua_State *L,tRet (tObj::*_f)())
{
  luaStack<tRet>::push(L, (luaStack<tObj*>::get(L,1)->*(_f))());
  return 1;
};

//const, no return
template <typename tObj, typename t0, typename t1, typename t2, typename t3>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2, t3) const)
{
  (luaStack<tObj*>::get(L,1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4),
     luaStack<t3>::get(L, 5));
  return 0;
};

template <typename tObj, typename t0, typename t1, typename t2>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2) const)
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4));
  return 0;
};

template <typename tObj, typename t0, typename t1>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1) const)
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3));
  return 0;
};

template <typename tObj, typename t0>
static int luaCall(lua_State *L, void (tObj::*_f)(t0) const) {
  (luaStack<tObj*>::get(L, 1)->*(_f))(luaStack<t0>::get(L, 2));
  return 0;
};

template <typename tObj>
static int luaCall(lua_State *L, void (tObj::*_f)() const)
{
  (luaStack<tObj*>::get(L, 1)->*(_f))();
  return 0;
};

//non const, no return
template <typename tObj, typename t0, typename t1, typename t2, typename t3, 
          typename t4, typename t5>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2, t3, t4, t5))
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4),
     luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6), luaStack<t5>::get(L, 7));
  return 1;
};

template <typename tObj, typename t0, typename t1, typename t2, typename t3, 
          typename t4>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2, t3, t4))
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4),
     luaStack<t3>::get(L, 5), luaStack<t4>::get(L, 6));
  return 1;
};

template <typename tObj, typename t0, typename t1, typename t2, typename t3>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2, t3)) 
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4),
     luaStack<t3>::get(L, 5));
  return 0;
};

template <typename tObj, typename t0, typename t1, typename t2>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1, t2))
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3), luaStack<t2>::get(L, 4));
  return 0;
};

template <typename tObj, typename t0, typename t1>
static int luaCall(lua_State *L, void (tObj::*_f)(t0, t1))
{
  (luaStack<tObj*>::get(L, 1)->*(_f))
    (luaStack<t0>::get(L, 2), luaStack<t1>::get(L, 3));
  return 0;
};
template <typename tObj, typename t0>
static int luaCall(lua_State *L, void (tObj::*_f)(t0))
{
  (luaStack<tObj*>::get(L,1)->*(_f))(luaStack<t0>::get(L, 2));
  return 0;
};

template <typename tObj>
static int luaCall(lua_State *L,void (tObj::*_f)())
{
  (luaStack<tObj*>::get(L,1)->*(_f))();
  return 0;
};

// actual bindings classes
class luaMethodBinding : public methodBinding{
 public:
  std::string _luaname;
  virtual int call (lua_State *L) = 0;
  luaMethodBinding(const std::string luaname){ _luaname = luaname; }
  luaMethodBinding(){ _luaname = ""; }
  virtual void getArgTypeNames(std::vector<std::string> &names){}
};

template <typename cb>
class methodBindingT :public luaMethodBinding {
 public:
  cb _f;
  methodBindingT(const std::string luaname, cb f) : luaMethodBinding(luaname) { _f = f; }
  int call(lua_State *L){ return luaCall(L, _f); }
  void getArgTypeNames(std::vector<std::string> &names){ argTypeNames<cb>::get(names); }
};
template <typename tObj, typename t0=void, typename t1=void, typename t2=void, 
          typename t3=void, typename t4=void, typename t5=void>
class constructorBindingT: public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                       luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
                                       luaStack<t4>::get(L, 5), luaStack<t5>::get(L, 6))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)(t0, t1, t2, t3, t4, t5)>::get(names);
  }
};


template <typename tObj>
class constructorBindingT<tObj, void, void, void, void,void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj()));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)()>::get(names);
  }
};

template <typename tObj, typename t0>
class constructorBindingT<tObj, t0, void, void, void, void,void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L,1);
    (luaStack<tObj*>::push(L,new tObj(luaStack<t0>::get(L,1))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names) {
    argTypeNames<void (tObj::*)(t0)>::get(names);
  }
};

template <typename tObj, typename t0, typename t1>
class constructorBindingT<tObj, t0, t1, void, void, void,void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)(t0, t1)>::get(names);
  }
};

template <typename tObj, typename t0, typename t1, typename t2>
class constructorBindingT<tObj, t0, t1, t2, void, void,void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                       luaStack<t2>::get(L, 3))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)(t0, t1, t2)>::get(names);
  }
};

template <typename tObj, typename t0, typename t1, typename t2,  typename t3>
class constructorBindingT<tObj, t0, t1, t2, t3, void, void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                       luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)(t0, t1, t2, t3)>::get(names);
  }
};

template <typename tObj, typename t0, typename t1, typename t2,  typename t3, typename t4>
class constructorBindingT<tObj, t0, t1, t2, t3, t4, void> : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    lua_remove(L, 1);
    (luaStack<tObj*>::push(L, new tObj(luaStack<t0>::get(L, 1), luaStack<t1>::get(L, 2),
                                       luaStack<t2>::get(L, 3), luaStack<t3>::get(L, 4),
                                       luaStack<t4>::get(L, 5))));
    return 1;
  }
  void getArgTypeNames(std::vector<std::string> &names)
  {
    argTypeNames<void (tObj::*)(t0, t1, t2, t3,t4)>::get(names);
  }
};

template <class t>
class destructorBindingT : public luaMethodBinding {
 public:
  int call(lua_State *L)
  {
    t *o = luaStack<t*>::get(L,1);
    delete o;
    return 0;
  }
};

class classBinding {
  std::string _className;
  binding *_b;
  luaMethodBinding *_constructor;
  static int callMethod(lua_State *L)
  {
    return static_cast<luaMethodBinding*>(lua_touserdata(L, lua_upvalueindex(1)))->call(L); 
  }
  // I'd like to remove the "luaMethodBinding" and the "methodBindingT"
  // classes and use this callback insteak but for some reason I don't
  // understand, it does not work
  static int callMethod2(lua_State *L)
  {
    int (*f)(lua_State*, void *) = (int (*)(lua_State*, void*))
      (lua_touserdata(L, lua_upvalueindex(1)));
    void *ff=lua_touserdata(L, lua_upvalueindex(2));
    return (*f)(L, ff);
  }
  static int tostring(lua_State *L)
  {
    typedef struct { void *pt; } userdata;
    char buff[32];
    const char *name = luaL_checkstring(L, lua_upvalueindex(1));
    userdata *ud = static_cast<userdata*>(lua_touserdata(L, 1));
    sprintf(buff, "%p", ud->pt);
    lua_pushfstring(L, "%s (%s)", name, buff);
    return 1;
  }
  void setConstructorLuaMethod(luaMethodBinding *constructor)
  {
    lua_State *L = _b->L;
    lua_getglobal(L, _className.c_str());
    int methods = lua_gettop(L);
    lua_getmetatable(L, methods);
    int mt = lua_gettop(L);
    lua_pushlightuserdata(L, (void*)constructor);
    lua_pushcclosure(L, callMethod, 1);
    lua_setfield(L, mt, "__call");
    lua_pop(L, 2);
    _constructor = constructor;
  }
  //for the doc
  std::string _description;
  classBinding *_parent;
 public:
  std::set<classBinding *> children;
  inline luaMethodBinding *getConstructor(){ return _constructor; }
  // get userdata from Lua stack and return pointer to T object
  classBinding(binding *b, std::string name)
  {
    _b = b;
    lua_State *L = _b->L;
    _className = name;
    _constructor = NULL;
    _parent = NULL;

    // there are 3 tables involved :
    // methods : the table of the C++ functions we bind (exept constructor)
    // metatable : the metatable attached to each intance of the
    //   class, falling back to method (metatable.__index=method)
    // mt : the metatable of method to store the constructor (__new)
    //   and possibly falling back to the parent metatable( __index)
    lua_newtable(L); // methods
    int methods = lua_gettop(L);
    lua_pushvalue(L, methods);
    lua_setglobal(L, _className.c_str()); // global[_className] = methods

    luaL_newmetatable(L, _className.c_str());
    int metatable = lua_gettop(L);

    lua_pushvalue(L, methods);
    lua_setfield(L, metatable, "__index"); // metatable.__index=methods

    lua_pushstring(L,name.c_str());
    lua_pushcclosure(L, tostring,1);
    lua_setfield(L, metatable, "__tostring");

    lua_newtable(L);
    //int mt = lua_gettop(L);

    lua_setmetatable(L, methods); // setmetatable(methods, mt)
    lua_pop(L, 2);  // drop metatable and method table
  } 
  void setParentClassName(const std::string parentClassName) {
    if(_parent)
      Msg::Error("Multiple inheritance not implemented in lua bindings "
                 "for class %s", _className.c_str());
    if(_b->classes.find(parentClassName) == _b->classes.end())
      Msg::Error("Unknown class %s", parentClassName.c_str());
    _parent = _b->classes[parentClassName];
    _parent->children.insert(this);
    lua_getglobal(_b->L, _className.c_str());
    lua_getmetatable(_b->L, -1);
    int mt=lua_gettop(_b->L);
    lua_getglobal(_b->L, parentClassName.c_str());
    lua_setfield(_b->L, mt, "__index"); 
    // mt.__index = global[_parentClassName] // this is the inheritance bit
    lua_pop(_b->L, 2);
  }
  template<typename parentType>
  void setParentClass()
  {
    setParentClassName(className<parentType>::get());
  }
  void setDescription(std::string description){ _description = description; }
  inline const std::string getDescription() const { return _description; }
  inline classBinding *getParent() const { return _parent; }
  std::map<std::string, luaMethodBinding *> methods;
  void addMethodLua (std::string n, luaMethodBinding *mb) {
    methods[n] = mb;
    lua_State *L = _b->L;
    lua_getglobal(L, _className.c_str());
    int methods = lua_gettop(L);
    /*int (*lc)(lua_State *,cb)=(int(*)(lua_State*,cb))luaCall;
    lua_pushlightuserdata(L, (void*)lc);
    lua_pushlightuserdata(L, (void*)f);
    lua_pushcclosure(L, callMethod2, 2);*/
    lua_pushlightuserdata(L, (void*)mb);
    lua_pushcclosure(L, callMethod, 1);
    lua_setfield(L,methods, n.c_str()); //className.name = callMethod(mb)
    lua_pop(L, 1);
  }
  template <typename cb>
  methodBinding *addMethod(std::string n, cb f)
  {
    luaMethodBinding *mb = new methodBindingT<cb>(n, f);
    addMethodLua(n,mb);
    return mb; 
  }
  template <typename tObj, typename t0, typename t1, typename t2, typename t3, 
            typename t4, typename t5>
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0,t1,t2,t3,t4,t5>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template <typename tObj, typename t0, typename t1, typename t2, typename t3, 
            typename t4>
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0,t1,t2,t3,t4>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template <typename tObj, typename t0, typename t1, typename t2, typename t3 >
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0,t1,t2,t3>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template <typename tObj, typename t0, typename t1, typename t2 >
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0,t1,t2>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template <typename tObj, typename t0, typename t1>
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0,t1>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template <typename tObj, typename t0>
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj,t0>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  template<typename tObj>
  methodBinding *setConstructor()
  {
    luaMethodBinding *constructorLua = new constructorBindingT<tObj>;
    setConstructorLuaMethod(constructorLua);
    return constructorLua;
  }
  inline const std::string getClassName()const {return _className;}
  ~classBinding() {
    if (_constructor)
      delete _constructor;
    for (std::map<std::string, luaMethodBinding *>::iterator it = methods.begin(); it!=methods.end(); it++) {
      delete it->second;
    }
  }
};

template<typename t>
classBinding *binding::addClass(std::string name)
{
  className<t>::set(name);
  classBinding *cb = new classBinding(this, name);
  luaMethodBinding *d = new destructorBindingT<t>();
  d->setDescription("destructor");
  cb->addMethodLua("delete", d);
  classes[name] = cb;
  return cb;
}

#endif
#endif
