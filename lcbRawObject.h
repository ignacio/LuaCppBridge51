#ifndef __luaCppBridge_RawObject_h__
#define __luaCppBridge_RawObject_h__

#include "LuaIncludes.h"
#include "lcbBaseObject.h"

#define LCB_RO_DECLARE_EXPORTABLE(classname) \
	static const LuaCppBridge::RawObject< classname >::RegType methods[];\
	static const char* className;

// acá la herencia no está andando. O saco el check() que chequea el tipo de userdata o hago algún injerto raro

namespace LuaCppBridge {

/**
Un RawObject es una clase de C++ expuesta hacia Lua como un userdata. 
Esto impide que desde Lua se agreguen cosas. Sólo se pueden utilizar las funciones provistas 
desde C++.

TO-DO:
Con esta clase NO se puede hacer herencia. Cuando se llama al método 'check', en los casos que hay 
herencia, se encuentra que el userdata que está en el stack no es del tipo que espera y falla. Habría que:
- eliminar ese chequeo (contra: puedo mandar cualquier fruta ahí y se reventaría por otro lado) o
- controlar que el tipo del userdata corresponda con un tipo que esté en la jerarquía de herencia
*/

template <typename T> class RawObject : public BaseObject<T, RawObject<T> > {
public:
	//////////////////////////////////////////////////////////////////////////
	///
	static void Register(lua_State* L) {
		Register(L, true);
	}

	static void Register(lua_State* L, bool isCreatableByLua)
	{
		int libraryTable = lua_gettop(L);
		luaL_checktype(L, libraryTable, LUA_TTABLE);	// must have library table on top of the stack
		lua_pushcfunction(L, RegisterLua);
		lua_pushvalue(L, libraryTable);
		lua_pushboolean(L, isCreatableByLua);
		lua_call(L, 2, 0);
	}

private:
	static int RegisterLua(lua_State* L) {
		luaL_checktype(L, 1, LUA_TTABLE);	// must pass a table
		bool isCreatableByLua = lua_toboolean(L, 2) != 0;
		
		int whereToRegister = 1;
		lua_newtable(L);
		int methods = lua_gettop(L);
		
		newmetatable(L, T::className);
		int metatable = lua_gettop(L);
		
		// store method table in globals so that scripts can add functions written in Lua.
		lua_pushstring(L, T::className);
		lua_pushvalue(L, methods);
		lua_settable(L, whereToRegister);
		
		// make getmetatable return the methods table
		lua_pushvalue(L, methods);
		lua_setfield(L, metatable, "__metatable");
		
		lua_pushliteral(L, "__index");
		lua_pushvalue(L, methods);
		lua_settable(L, metatable);
		
		lua_pushliteral(L, "__tostring");
		lua_pushcfunction(L, tostring_T);
		lua_settable(L, metatable);
		
		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gc_T);
		lua_settable(L, metatable);
		
		if(isCreatableByLua) {
			// hago que llamando al nombre de la clase, me construya un objeto
			lua_newtable(L);				// mt for method table
			lua_pushcfunction(L, T::new_T);
			lua_pushvalue(L, -1);			// dup new_T function
			set(L, methods, "new");			// add new_T to method table
			set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		else {
			// hago que llamando al nombre de la clase, me salte un error
			lua_newtable(L);				// mt for method table
			lua_pushcfunction(L, forbidden_new_T);
			lua_pushvalue(L, -1);			// dup new_T function
			set(L, methods, "new");			// add new_T to method table
			set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		
		// fill method table with methods from class T
		for(const RegType* l = T::methods; l->name; l++) {
			lua_pushstring(L, l->name);
			lua_pushlightuserdata(L, (void*)l);
			lua_pushcclosure(L, thunk_methods, 1);
			lua_settable(L, methods);
		}
		
		lua_pop(L, 2);  // drop metatable and method table
		
		return 0;
	}
};

}; // end of the namespace

#endif
