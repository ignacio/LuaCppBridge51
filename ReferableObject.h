#if !defined(AFX_REFERABLEOBJECT_H__64D50744_032B_4323_9227_A2B13C3EB601__INCLUDED_)
#define AFX_REFERABLEOBJECT_H__64D50744_032B_4323_9227_A2B13C3EB601__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "LuaIncludes.h"

namespace LuaCppBridge {

class ReferableObject {
public:
	// este método se podría borrar (en la vbctrls no se usa)
	int GetRef(lua_State* L) {
		int t;
		__asm int 3;
		lua_pushstring(L,"_myref");
		lua_gettable(L,-2);
		t = (int)lua_tonumber(L,-1);
		lua_pop(L,1);
		return t;
	}
	
	ReferableObject(lua_State* L) {
	}

	
	void dumpTable(lua_State* L) {
		lua_pushnil(L);
		const char* v;
		int t;
		t = lua_gettop(L);
		while(lua_next(L, -2) != 0) {
			t = lua_gettop(L);
			//k = lua_tostring(L,-2);
			lua_pushvalue(L, -2);
			v = lua_typename(L, lua_type(L, -1));
			switch(lua_type(L, -1)) {
				case LUA_TNUMBER:
				case LUA_TSTRING:
					v = lua_tostring(L, -1);
				break;
			}
			lua_pop(L, 1);

			t = lua_gettop(L);
			v = lua_typename(L, lua_type(L, -1));
			switch(lua_type(L, -1)) {
				case LUA_TNUMBER:
				case LUA_TSTRING:
					v = lua_tostring(L, -1);
				break;
			}
			lua_pop(L, 1);
		}
	}
	
	/*int getTypeProperty(lua_State* L, const char* name) {
		GetSelf(L, ref_to_self);
		lua_pushstring(L, name);
		lua_gettable(L, -2);
		int value = lua_type(L, -1);
		lua_pop(L, 2);
		return value;
	}*/

	/*double getDoubleProperty(lua_State* L, const char* name) {
		GetSelf(L, ref_to_self);
		lua_pushstring(L, name);
		lua_gettable(L, -2);
		double value = lua_tonumber(L, -1);
		lua_pop(L, 2);
		return value;
	}*/

	// como dejar esto en un .h sin que dependa de CString y sin hacer un strdup ?
	/*CString getStringProperty(lua_State* L, const char* name)
	{
		GetSelf(L, ref_to_self);
		lua_pushstring(L, name);
		lua_gettable(L, -2);
		CString value = lua_tostring(L, -1);
		lua_pop(L, 2);
		return value;
	}*/
};

}; // end of the namespace

#endif // !defined(AFX_REFERABLEOBJECT_H__64D50744_032B_4323_9227_A2B13C3EB601__INCLUDED_)
