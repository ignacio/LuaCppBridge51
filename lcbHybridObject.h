#ifndef __luaCppBridge_HybridObject_h__
#define __luaCppBridge_HybridObject_h__

#include "LuaIncludes.h"
#include "lcbBaseObject.h"

#define LCB_HO_DECLARE_EXPORTABLE(classname) \
	static const LuaCppBridge::HybridObject< classname >::RegType methods[];\
	static const char* className;

namespace LuaCppBridge {

/**
An HybridObject is a C++ class whose instances are exposed to Lua as tables. It differs from RawObject 
in that additional methods and members can be added dynamically to its instances.
*/
template <class T> class HybridObject : public BaseObject<T, HybridObject<T> > {
private:
	typedef BaseObject<T, HybridObject<T> > base_type;
public:
	typedef typename base_type::RegType RegType;
	typedef typename base_type::userdataType userdataType;

	typedef struct ObjectWrapper {
		T* wrappedObject;
		bool collectable;
	} ObjectWrapper;

	//using base_type::forbidden_new_T;
	//using base_type::RegType;
	

public:
	//////////////////////////////////////////////////////////////////////////
	///
	static void Register (lua_State* L, const char* parentClassName) {
		Register(L, parentClassName, true);
	}

	static void Register (lua_State* L, const char* parentClassName, bool isCreatableByLua)
	{
		int libraryTable = lua_gettop(L);
		luaL_checktype(L, libraryTable, LUA_TTABLE);	// must have library table on top of the stack
		lua_pushcfunction(L, RegisterLua);
		lua_pushvalue(L, libraryTable);
		lua_pushstring(L, parentClassName);
		lua_pushboolean(L, isCreatableByLua);
		lua_call(L, 3, 0);
	}

	// pcall named lua method from userdata method table
	static int pcall (lua_State* L, const char* method, int nargs = 0, int nresults = LUA_MULTRET, int errfunc = 0)
	{
		int base = lua_gettop(L) - nargs;	// userdata index
		if(!lua_istable(L, base)) {
			lua_settop(L, base-1);			// drop table and args
			luaL_error(L, "not a valid %s table", T::className);
			return -1;
		}
		
		lua_pushstring(L, method);			// method name
		lua_gettable(L, base);				// get method from table
		if(lua_isnil(L, -1)) {				// no method?
			lua_settop(L, base - 1);		// drop userdata and args
			//lua_pushfstring(L, "%s missing method '%s'", T::className, method);
			return -1;
		}
		lua_insert(L, base);				// put method under userdata, args
		// so the stack now is: method, self, args
		
		int status = lua_pcall(L, 1 + nargs, nresults, errfunc);	// call method
		if(status) {
			const char* msg = lua_tostring(L, -1);
			if(msg == NULL) {
				msg = "(error with no message)";
			}
			lua_pushfstring(L, "%s:%s status = %d\n%s", T::className, method, status, msg);
			lua_remove(L, base);			// remove old message
			return -1;
		}
		return lua_gettop(L) - base + 1;	// number of results
	}

	// call named lua method from userdata method table
	static int call (lua_State* L, const char* method, int nargs = 0, int nresults = LUA_MULTRET)
	{
		int base = lua_gettop(L) - nargs;	// userdata index
		if(!lua_istable(L, base)) {
			lua_settop(L, base-1);			// drop table and args
			luaL_error(L, "not a valid %s table", T::className);
			return -1;
		}
		
		lua_pushstring(L, method);			// method name
		lua_gettable(L, base);				// get method from table
		if(lua_isnil(L, -1)) {				// no method?
			lua_settop(L, base - 1);		// drop userdata and args
			//lua_pushfstring(L, "%s missing method '%s'", T::className, method);
			return -1;
		}
		lua_insert(L, base);				// put method under userdata, args
		// so the stack now is: method, self, args
		
		lua_call(L, 1 + nargs, nresults);	// call method
		return lua_gettop(L) - base + 1;	// number of results
	}
	
	// push onto the Lua stack a table containing a userdata which itself contains a pointer to T object
	static int push (lua_State* L, T* obj, bool gc = false) {
		if(!obj) {
			lua_pushnil(L);
			return 0;
		}
		getmetatable(L, T::className);	// lookup metatable in Lua registry
		if(lua_isnil(L, -1)) {
			luaL_error(L, "%s missing metatable", T::className);
		}
		// stack: metatable
		int metatable = lua_gettop(L);
		base_type::subtable(L, metatable, "userdata", "v");
		// stack: metatable, table userdata
		int newTable = pushtable(L, obj);	// push the table I'll return to Lua on the stack
		// stack: metatable, table userdata, new table
		lua_pushnumber(L, 0);	// store a pointer to the object at index 0
		
		// create a userdata, whose content is a lightweight wrapper to my object
		ObjectWrapper* wrapper = static_cast<ObjectWrapper*>(lua_newuserdata(L, sizeof(ObjectWrapper)));
		wrapper->wrappedObject = obj;
		wrapper->collectable = gc;
		
		// set its metatable (I'm only interested in the __gc method)
		lua_pushvalue(L, metatable);
		lua_setmetatable(L, -2);
		
		lua_settable(L, newTable);
		
		lua_pushvalue(L, metatable);	// copy the metatable
		lua_setmetatable(L, -2);		// and use it as metatable for the Lua table
		
		lua_replace(L, metatable);		// put the new table in place of the metatable on the stack
		lua_settop(L, metatable);
		return metatable;				// index of new table
	}
	
	// get userdata from Lua stack and return pointer to T object
	static T* check (lua_State* L, int narg) {
		luaL_checktype(L, narg, LUA_TTABLE);
		lua_pushnumber(L, 0);
		lua_rawget(L, narg);
		luaL_checktype(L, -1, LUA_TUSERDATA);
		T* pT = static_cast<ObjectWrapper*>(lua_touserdata(L, -1))->wrappedObject;
		if(!pT) {
			luaL_typerror(L, narg, T::className);
		}
		lua_pop(L, 1);
		return pT;	// pointer to T object
	}

	// test if the value in the given position in the stack is a T object
	static bool test (lua_State* L, int narg) {
		if(!lua_istable(L, narg)) {
			return false;
		}
		lua_pushnumber(L, 0);
		lua_rawget(L, narg);
		if(!lua_isuserdata(L, -1)) {
			lua_pop(L, 1);
			return false;
		}
		T* pT = static_cast<ObjectWrapper*>(lua_touserdata(L, -1))->wrappedObject;
		if(!pT) {
			lua_pop(L, 1);
			return false;
		}
		lua_pop(L, 1);
		return true;
	}
	
protected:
	// create a new T object and push onto the Lua stack a table containing a userdata which itself contains a pointer to T object
	static int new_T (lua_State* L) {
		lua_remove(L, 1);	// use classname:new(), instead of classname.new()
		T* obj = new T(L);	// call constructor for T objects
		int newTable = push(L, obj, true); // gc_T will delete this object
		if(base_type::s_trackingEnabled) {
			obj->KeepTrack(L);
		}
		
		// if this method was called with a table as parameter, then copy its values to the newly created object
		if(lua_gettop(L) == 2 && lua_istable(L, 1)) {
			lua_pushnil(L);
			while(lua_next(L, 1)) {
				lua_pushvalue(L, -2);
				lua_insert(L, -2);
				lua_settable(L, newTable);
			}
		}
		// last step in the creation of the object. call a method that can access the userdata that will be sent 
		// back to Lua
		obj->PostConstruct(L);
		return 1;
	}

	// garbage collection metamethod, comes with the userdata on top of the stack
	static int gc_T (lua_State* L) {
#ifdef ENABLE_TRACE
	char buff[256];
		sprintf(buff, "attempting to collect object of type '%s'\n", T::className);
		OutputDebugString(buff);
#endif
		ObjectWrapper* wrapper = static_cast<ObjectWrapper*>(lua_touserdata(L, -1));
		if(wrapper->collectable && wrapper->wrappedObject) {
#ifdef ENABLE_TRACE
			sprintf(buff, "collected %s (%p)\n", T::className, wrapper->wrappedObject);
			OutputDebugString(buff);
#endif
			delete wrapper->wrappedObject;	// call destructor for wrapped objects
		}
		return 0;
	}
	
	static int tostring_T (lua_State* L) {
		// watch out, both the userdata and the table share this method
		char buff[32];
		if(lua_istable(L, 1)) {
			lua_pushnumber(L, 0);
			lua_rawget(L, 1);
			luaL_checktype(L, -1, LUA_TUSERDATA);
		}
		const T* pT = static_cast<ObjectWrapper*>(lua_touserdata(L, -1))->wrappedObject;
		sprintf(buff, "%p", pT);
		lua_pushfstring(L, "%s (%s)", T::className, buff);
		return 1;
	}

private:
	static int RegisterLua (lua_State* L) {
		luaL_checktype(L, 1, LUA_TTABLE);	// must pass a table
		const char* parentClassName = luaL_optstring(L, 2, NULL);
		bool isCreatableByLua = lua_toboolean(L, 3) != 0;

		int whereToRegister = 1;
		// creo una tabla para los métodos
		lua_newtable(L);
		int methods = lua_gettop(L);
			
		newmetatable(L, T::className);
		int metatable = lua_gettop(L);
			
		// store method table in module so that scripts can add functions written in Lua.
		lua_pushvalue(L, methods);
		base_type::set(L, whereToRegister, T::className);
		
		// make getmetatable return the methods table
		lua_pushvalue(L, methods);
		lua_setfield(L, metatable, "__metatable");
			
		lua_pushvalue(L, methods);
		base_type::set(L, metatable, "__index");
			
		lua_pushcfunction(L, T::tostring_T);
		base_type::set(L, metatable, "__tostring");
			
		lua_pushcfunction(L, gc_T);
		base_type::set(L, metatable, "__gc");
			
		if(isCreatableByLua) {
			// Make Classname() and Classname:new() construct an instance of this class
			lua_newtable(L);							// mt for method table
			lua_pushcfunction(L, T::new_T);
			lua_pushvalue(L, -1);						// dup new_T function
			base_type::set(L, methods, "new");			// add new_T to method table
			base_type::set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		else {
			// Both Make Classname() and Classname:new() will issue an error
			lua_newtable(L);							// mt for method table
			lua_pushcfunction(L, base_type::forbidden_new_T);
			lua_pushvalue(L, -1);						// dup new_T function
			base_type::set(L, methods, "new");			// add new_T to method table
			base_type::set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}

		// fill method table with methods from class T
		for(const RegType* l = T::methods; l->name; l++) {
			lua_pushstring(L, l->name);
			lua_pushlightuserdata(L, (void*)l);
			lua_pushcclosure(L, base_type::thunk_methods, 1);
			lua_settable(L, methods);
		}
		// If a parent class was supplied, make it inherit from it
		if(parentClassName) {
			// do this:
			// getmetatable(T::className).__index = className
			// ie: getmetatable(VbButton).__index = CVbCtrlObj
			lua_getmetatable(L, methods);
			lua_pushliteral(L, "__index");
			lua_getfield(L, whereToRegister, parentClassName);
			if(lua_isnil(L, -1)) {
				luaL_error(L, "class '%s' is not defined", parentClassName);
			}
			lua_rawset(L, -3);
			lua_pop(L, 1);
		}
		
		lua_pop(L, 2);	// drop metatable and method table
		return 0;
	}
	
public:
	void PostConstruct (lua_State* L) {};
};

}; // end of the namespace

#endif
