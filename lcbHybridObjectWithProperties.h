#ifndef __luaCppBridge_HybridObjectWithProperties_h__
#define __luaCppBridge_HybridObjectWithProperties_h__

#include "LuaIncludes.h"
#include "lcbBaseObject.h"

#define LCB_HOWP_DECLARE_EXPORTABLE(classname) \
	static const LuaCppBridge::HybridObjectWithProperties< classname >::RegType methods[];\
	static const LuaCppBridge::HybridObjectWithProperties< classname >::RegType setters[];\
	static const LuaCppBridge::HybridObjectWithProperties< classname >::RegType getters[];\
	static const char* className;

/**
Algunos macros �tiles m�s que nada para la definici�n de propiedades.
*/
#define LCB_DECL_SETGET(fieldname) int set_##fieldname (lua_State* L); int get_##fieldname (lua_State*L);
#define LCB_DECL_GET(fieldname) int get_##fieldname (lua_State* L);
#define LCB_DECL_SET(fieldname) int set_##fieldname (lua_State* L);
#define LCB_ADD_SET(classname, fieldname) { #fieldname , &classname::set_##fieldname }
#define LCB_ADD_GET(classname, fieldname) { #fieldname , &classname::get_##fieldname }
#define LCB_IMPL_SET(classname, fieldname) int classname::set_##fieldname (lua_State* L)
#define LCB_IMPL_GET(classname, fieldname) int classname::get_##fieldname (lua_State* L)

namespace LuaCppBridge {

/**
Un HybridObjectWithProperties es una clase de C++ expuesta hacia Lua como un userdata. A diferencia de 
RawObjectWithProperties, se pueden agregar propiedades din�micamente, como si fuese una tabla. Estas viven 
en una tabla aparte (vinculada al userdata).

Esta clase tiene sutilezas. 
Por ejemplo, si para una property solo definimos un getter, entonces es de solo lectura. En ese caso escribir en 
esa property va a dar error.
Desde Lua va a ocurrir lo siguiente:
print(obj.test) -- gives the 'internal' value
obj.test = "something"	-- error!


TO-DO:
Con esta clase NO se puede hacer herencia. Desde Lua no logr� que se viesen las propiedades del padre. 
S� consegu� que se pudiese acceder a los m�todos del padre nom�s, pero no ten�a sentido habilitarlo si no se 
puede acceder a las propiedades.
*/
template <typename T> class HybridObjectWithProperties : public BaseObject<T, HybridObjectWithProperties<T> > {
private:
	typedef BaseObject<T, HybridObjectWithProperties<T> > base_type;
public:
	typedef typename base_type::RegType RegType;
	typedef typename base_type::userdataType userdataType;

public:
	//////////////////////////////////////////////////////////////////////////
	///
	static void Register(lua_State* L, const char* parentClassName) {
		Register(L, parentClassName, true);
	}

	static void Register(lua_State* L, const char* parentClassName, bool isCreatableByLua)
	{
		int libraryTable = lua_gettop(L);
		luaL_checktype(L, libraryTable, LUA_TTABLE);	// must have library table on top of the stack
		lua_pushcfunction(L, RegisterLua);
		lua_pushvalue(L, libraryTable);
		lua_pushstring(L, parentClassName);
		lua_pushboolean(L, isCreatableByLua);
		lua_call(L, 3, 0);
	}

	// create a new T object and push onto the Lua stack a userdata containing a pointer to T object
	static int new_T(lua_State* L) {
		lua_remove(L, 1);	// use classname:new(), instead of classname.new()
		T* obj = new T(L);	// call constructor for T objects
		int newObject = push(L, obj, true); // gc_T will delete this object
		if(T::s_trackingEnabled) {
			obj->KeepTrack(L);
		}
		
		// if this method was called with a table as parameter, then copy its values to the newly created object
		if(lua_gettop(L) == 2 && lua_istable(L, 1)) {
			lua_pushnil(L);					// stack: table, newObject, nil
			while(lua_next(L, 1)) {			// stack: table, newObject, key, value
				lua_pushvalue(L, -2);		// stack: table, newObject, key, value, key
				lua_insert(L, -2);			// stack: table, newObject, key, key, value
				lua_settable(L, newObject);	// stack: table, newObject, key
			}
		}
		
		// last step in the creation of the object. call a method that can access the userdata that will be sent 
		// back to Lua
		obj->PostConstruct(L);
		return 1;			// userdata containing pointer to T object
	}
	
	// push onto the Lua stack a userdata containing a pointer to T object
	// int ret = BaseObject<T, HybridObjectWithProperties<T> >::push(L, obj, gc);
	static int push(lua_State* L, T* obj, bool gc = false) {
		if(!obj) {
			lua_pushnil(L);
			return 0;
		}
		getmetatable(L, T::className);	// look for the metatable
		if(lua_isnil(L, -1)) {
			luaL_error(L, "%s missing metatable", T::className);
		}
		int mt = lua_gettop(L);
		T::subtable(L, mt, "userdata", "v");
		userdataType *ud = static_cast<userdataType*>(pushuserdata(L, obj, sizeof(userdataType)));
		if(ud) {
			// set up a table as the userdata environment
			lua_newtable(L);
			lua_setfenv(L, -2);
			ud->pT = obj;  // store pointer to object in userdata
			lua_pushvalue(L, mt);
			lua_setmetatable(L, -2);
			ud->collectable = gc;
		}
		lua_replace(L, mt);
		lua_settop(L, mt);
		return mt;	// index of userdata containing pointer to T object
	}

	// push onto the Lua stack a userdata containing a pointer to T object. The object is not collectable and is unique (two 
	// calls to push_unique will yield two different userdatas)
	static int push_unique(lua_State* L, T* obj) {
		if(!obj) {
			lua_pushnil(L);
			return 0;
		}
		getmetatable(L, T::className);	// look for the metatable
		if(lua_isnil(L, -1)) {
			luaL_error(L, "%s missing metatable", T::className);
		}
		int mt = lua_gettop(L);
		userdataType *ud = static_cast<userdataType*>(lua_newuserdata(L, sizeof(userdataType)));	// create new userdata
		if(ud) {
			// set up a table as the userdata environment
			lua_newtable(L);
			lua_setfenv(L, -2);
			ud->pT = obj;  // store pointer to object in userdata
			ud->collectable = false;
			lua_pushvalue(L, mt);
			lua_setmetatable(L, -2);
		}
		lua_replace(L, mt);
		lua_settop(L, mt);
		return mt;	// index of userdata containing pointer to T object
	}
	
protected:
	/**
	__index metamethod. Looks for a a dynamic property, then for a property, then for a method.
	upvalues:	1 = table with get properties
				2 = table with methods
	initial stack: self (userdata), key
	*/
	static int thunk_index(lua_State* L) {
		T* obj = base_type::check(L, 1);	// get 'self', or if you prefer, 'this'
		lua_getfenv(L, 1);					// stack: userdata, key, userdata_env
		lua_pushvalue(L, 2);				// stack: userdata, key, userdata_env, key
		lua_rawget(L, 3);
		if(!lua_isnil(L, -1)) {
			// found something, return it
			return 1;
		}
		lua_pop(L, 2);					// stack: userdata, key
		lua_pushvalue(L, 2);			// stack: userdata, key, key
		lua_rawget(L, lua_upvalueindex(1));
		if(!lua_isnil(L, -1)) {			// found a property
			// stack: userdata, key, getter (RegType*)
			RegType* l = static_cast<RegType*>(lua_touserdata(L, -1));
			//lua_settop(L, 0);
			lua_settop(L, 1);
			return (obj->*(l->mfunc))(L);  // call member function
		}
		else {
			// not a property, look for a method
			lua_pop(L, 1);				// stack: userdata, key
			lua_pushvalue(L, 2);		// stack: userdata, key, key
			lua_rawget(L, lua_upvalueindex(2));
			if(!lua_isnil(L, -1)) {
				// found the method, return it
				return 1;
			}
			else {
				// Aca deber�a seguir buscando para arriba en la metatabla del padre (si es que estoy)
				// heredando, pero NPI de c�mo se hace, as� que queda por esta
				lua_pushnil(L);
				return 1;
			}
		}
		return 0;
	}
	
	/**
	__newindex metamethod. Looks for a set property, else it sets the value as a dynamic property.
	upvalues:	1 = table with set properties
	initial stack: self (userdata), key, value
	*/
	static int thunk_newindex(lua_State* L) {
		T* obj = base_type::check(L, 1);	// get 'self', or if you prefer, 'this'

		// look if there is a setter for 'key'
		lua_pushvalue(L, 2);				// stack: userdata, key, value, key
		lua_rawget(L, lua_upvalueindex(1));
		if(!lua_isnil(L, -1)) {
											// stack: userdata, key, value, setter
			RegType* p = static_cast<RegType*>(lua_touserdata(L, -1));
			lua_pop(L, 1);					// stack: userdata, key, value
			return (obj->*(p->mfunc))(L);	// call member function
		}
		else {
			// there is no setter. If there is a getter, then this property is read only.
											// stack: userdata, key, value, nil
			lua_pop(L, 1);					// stack: userdata, key, value
			lua_pushvalue(L, 2);			// stack: userdata, key, value, key
			lua_rawget(L, lua_upvalueindex(2));	// stack: userdata, key, value, getter?
			if(!lua_isnil(L, -1)) {
				lua_pop(L, 1);
				return luaL_error(L, "trying to write to read only property '%s'", lua_tostring(L, 2));
			}
			// sets 'value' in the environment
			lua_pop(L, 1);					// stack: userdata, key, value
			lua_getfenv(L, 1);				// stack: userdata, key, value, userdata_env
			lua_replace(L, 1);				// stack: userdata_env, key, value
			lua_rawset(L, 1);
		}
		return 0;
	}

private:
	static int RegisterLua(lua_State* L) {
		luaL_checktype(L, 1, LUA_TTABLE);	// must pass a table
		//const char* parentClassName = luaL_optstring(L, 2, NULL);
		bool isCreatableByLua = lua_toboolean(L, 3) != 0;
		
		int whereToRegister = 1;
		lua_newtable(L);
		int methods = lua_gettop(L);
		
		newmetatable(L, T::className);
		int metatable = lua_gettop(L);
		
		// store method table in globals so that scripts can add functions written in Lua.
		lua_pushvalue(L, methods);
		base_type::set(L, whereToRegister, T::className);
		
		// make getmetatable return the methods table
		lua_pushvalue(L, methods);
		lua_setfield(L, metatable, "__metatable");
		
		// lunar mio
		lua_newtable(L);						// getters, "__index"
		int getters_index = lua_gettop(L);
		lua_pushliteral(L, "__index");			// getters, "__index"
		lua_pushvalue(L, getters_index);		// getters, "__index", getters
		for(const RegType* l = T::getters; l->name; l++) {
			lua_pushstring(L, l->name);			// getters, "__index", getters, name
			lua_pushlightuserdata(L, (void*)l); // getters, "__index", getters, name, property implementation
			lua_settable(L, getters_index);		// getters, "__index", getters
		}
		lua_pushvalue(L, methods);				// getters, "__index", getters, methods
		lua_pushcclosure(L, thunk_index, 2);	// getters, "__index", thunk
		lua_settable(L, metatable);				// getters
		
		
		lua_pushliteral(L, "__newindex");		// getters, "__newindex"
		lua_newtable(L);						// getters, "__newindex", setters
		int newindex = lua_gettop(L);
		for (const RegType* setter = T::setters; setter->name; setter++) {
			lua_pushstring(L, setter->name);			// getters, "__newindex", setters, name
			lua_pushlightuserdata(L, (void*)(setter));	// getters, "__newindex", setters, name, property implementation
			lua_settable(L, newindex);					// getters, "__newindex", setters
		}
		lua_pushvalue(L, getters_index);		// getters, "__newindex", setters, getters
		lua_pushcclosure(L, thunk_newindex, 2);	// getters, "__newindex", thunk
		lua_settable(L, metatable);				// getters
		lua_pop(L, 1);	// remove the getters table
		
		lua_pushcfunction(L, T::tostring_T);
		base_type::set(L, metatable, "__tostring");
		
		lua_pushcfunction(L, base_type::gc_T);
		base_type::set(L, metatable, "__gc");
		
		if(isCreatableByLua) {
			// hago que llamando al nombre de la clase, me construya un objeto
			lua_newtable(L);				// mt for method table
			lua_pushcfunction(L, T::new_T);
			lua_pushvalue(L, -1);			// dup new_T function
			base_type::set(L, methods, "new");			// add new_T to method table
			base_type::set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		else {
			// hago que llamando al nombre de la clase, me salte un error
			lua_newtable(L);							// mt for method table
			lua_pushcfunction(L, base_type::forbidden_new_T);
			lua_pushvalue(L, -1);						// dup new_T function
			base_type::set(L, methods, "new");			// add new_T to method table
			base_type::set(L, -3, "__call");			// mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		
		// fill method table with methods from class T
		for (const RegType* method = T::methods; method->name; method++) {
			lua_pushstring(L, method->name);
			lua_pushlightuserdata(L, (void*)method);
			lua_pushcclosure(L, base_type::thunk_methods, 1);
			lua_settable(L, methods);
		}
		lua_pop(L, 2);  // drop metatable and method table

		return 0;
	}

public:
	void PostConstruct(lua_State* L) {};
};

}; // end of the namespace

#endif
