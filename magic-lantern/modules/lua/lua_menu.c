/***
 Functions for interacting with the ML menu
 
 @author Magic Lantern Team
 @copyright 2014
 @license GPL
 @module menu
 */

#include <dryos.h>
#include <string.h>
#include <menu.h>
#include <property.h>

#include "lua_common.h"

extern int menu_redraw_blocked;
static int luaCB_menu_instance_index(lua_State * L);
static int luaCB_menu_instance_newindex(lua_State * L);
static int luaCB_menu_remove(lua_State * L);
static void load_menu_entry(lua_State * L, struct script_menu_entry * script_entry, struct menu_entry * menu_entry, const char * default_name);

static void lua_menu_task(lua_State * L)
{
    if(L)
    {
        struct semaphore * sem = NULL;
        if(!lua_take_semaphore(L, 0, &sem) && sem)
        {
            int isnum = 0;
            int arg_count = lua_tointegerx(L, -1, &isnum);
            if(isnum) lua_pop(L,1);
            else arg_count = 0;
            printf("running script...\n");
            if(docall(L, arg_count, 0))
            {
                printf("script failed:\n %s\n", lua_tostring(L, -1));
            }
            else
            {
                printf("script finished\n");
            }
            give_semaphore(sem);
        }
        else
        {
            printf("lua semaphore timeout: lua_menu_task (%dms)\n", 0);
        }
    }
}

//copied from lua.c
static int msghandler (lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                  luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

int docall (lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

static MENU_SELECT_FUNC(script_menu_select)
{
    struct script_menu_entry * script_entry = priv;
    if(script_entry && script_entry->L && script_entry->select_ref != LUA_NOREF)
    {
        lua_State * L = script_entry->L;
        struct semaphore * sem = NULL;
        if(!lua_take_semaphore(L, 500, &sem) && sem)
        {
            if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->select_ref) == LUA_TFUNCTION)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->self_ref);
                lua_pushinteger(L, delta);
                if(script_entry->run_in_separate_task)
                {
                    static int lua_task_id = 0;
                    char task_name[32];
                    snprintf(task_name,32,"lua_menu_task[%d]",lua_task_id++);
                    lua_pushinteger(L, 2); //push the arg count onto the stack
                    give_semaphore(sem); //give the semaphore back (the task will take it)
                    task_create(task_name, 0x1c, 0x8000, lua_menu_task, L);
                }
                else
                {
                    if(docall(L, 2, 0))
                    {
                        fprintf(stderr, "script error:\n %s\n", lua_tostring(L, -1));
                    }
                    give_semaphore(sem);
                }
            }
            else
            {
                fprintf(stderr, "select is not a function\n");
                give_semaphore(sem);
            }
        }
        else
        {
            printf("lua semaphore timeout: menu.select (%dms)\n", 500);
        }
    }
}

static MENU_UPDATE_FUNC(script_menu_update)
{
    struct script_menu_entry * script_entry = entry->priv;
    if(script_entry && script_entry->L)
    {
        lua_State * L = script_entry->L;
        struct semaphore * sem = NULL;
        if(!lua_take_semaphore(L, 100, &sem) && sem)
        {
            if (script_entry->menu_entry->children ||
                script_entry->menu_entry->icon_type == IT_ACTION)
            {
                /* by default, menus with submenus do not display a value */
                /* same for menus with ICON_TYPE.ACTION */
                MENU_SET_VALUE("");
            }
            if(script_entry->update_ref != LUA_NOREF)
            {
                if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->update_ref) == LUA_TFUNCTION)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->self_ref);
                    if(!docall(L, 1, 1) && lua_isstring(L, -1))
                    {
                        MENU_SET_VALUE("%s", lua_tostring(L, -1));
                    }
                }
                else if(lua_isstring(L, -1))
                {
                    MENU_SET_VALUE("%s", lua_tostring(L, -1));
                }
                lua_pop(L,1);
            }
            if(script_entry->info_ref != LUA_NOREF)
            {
                if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->info_ref) == LUA_TFUNCTION)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->self_ref);
                    if(!docall(L, 1, 1) && lua_isstring(L, -1))
                    {
                        MENU_SET_WARNING(MENU_WARN_INFO, "%s", lua_tostring(L, -1));
                    }
                }
                else if(lua_isstring(L, -1))
                {
                    MENU_SET_WARNING(MENU_WARN_INFO, "%s", lua_tostring(L, -1));
                }
                lua_pop(L,1);
            }
            if(script_entry->rinfo_ref != LUA_NOREF)
            {
                if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->rinfo_ref) == LUA_TFUNCTION)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->self_ref);
                    if(!docall(L, 1, 1) && lua_isstring(L, -1))
                    {
                        MENU_SET_RINFO("%s", lua_tostring(L, -1));
                    }
                }
                else if(lua_isstring(L, -1))
                {
                    MENU_SET_RINFO("%s", lua_tostring(L, -1));
                }
                lua_pop(L,1);
            }
            if(script_entry->warning_ref != LUA_NOREF)
            {
                if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->warning_ref) == LUA_TFUNCTION)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->self_ref);
                    if(!docall(L, 1, 1) && lua_isstring(L, -1))
                    {
                        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "%s", lua_tostring(L, -1));
                    }
                }
                lua_pop(L,1);
            }
            give_semaphore(sem);
        }
        else
        {
            printf("lua semaphore timeout: menu.update (%dms)\n", 100);
        }
    }
}

static int lua_hasfield(lua_State * L, int idx, const char * name, int expected_type)
{
    int result = lua_getfield(L, -1, name) == expected_type;
    lua_pop(L, 1);
    return result;
}

static int get_index_for_choices(struct menu_entry * menu_entry, const char * value)
{
    int i;
    for(i = 0; i <= menu_entry->max; i++)
    {
        if(!strcmp(menu_entry->choices[i], value))
        {
            return i;
        }
    }
    return 0;
}

const char * lua_menu_instance_fields[] =
{
    "value",
    "name",
    "help",
    "help2",
    "advanced",
    "depends_on",
    "edit_mode",
    "hidden",
    "icon_type",
    "jhidden",
    "max",
    "min",
    "selected",
    "shidden",
    "starred",
    "submenu_height",
    "submenu_width",
    "unit",
    "works_best_in",
    "run_in_separate_task",
    "select",
    "update",
    "info",
    "rinfo",
    "warning",
    NULL
};

/*** Creates a new menu item
 @tparam table definition
 @function new
 @return a menu object
 @usage
mymenu = menu.new
{
    parent = "Scripts",
    name = "Lua Test Script",
    help = "Some help for this script",
    value = 1,
    submenu =
    { 
        {
            name = "Run",
            help = "Run this script.",
            icon_type = ICON_TYPE.ACTION,
            update = "",
        },
        {
            name = "Parameter Example",
            help = "Help for Parameter Example",
            min = 0,
            max = 100,
            unit = UNIT.DEC,
            warning = function(this) if this.value == 5 then return "this value is not supported" end end,
        },
        {
            name = "Choices Example",
            choices = { "choice1", "choice2", "choice3" },
        }
    },
    update = function(this) return this.submenu["Choices Example"].value end,
}

mymenu.submenu["Run"].select = function()
    print("Parameter Example= "..mymenu.submenu["Parameter Example"].value)
    print("Choices Example= "..mymenu.submenu["Choices Example"].value)
end
*/
static int luaCB_menu_new(lua_State * L)
{
    if(!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected table");
    
    lua_pushvalue(L, 1);
    const char * parent = LUA_FIELD_STRING("parent", "Scripts");
    lua_pop(L, 1);
    
    struct script_menu_entry * new_entry = lua_newuserdata(L, sizeof(struct script_menu_entry));
    lua_pushvalue(L, -1);
    new_entry->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    //add a metatable to the userdata object for value lookups and to store submenu
    lua_newtable(L);
    lua_pushcfunction(L, luaCB_menu_instance_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, luaCB_menu_instance_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, luaCB_pairs);
    lua_setfield(L, -2, "__pairs");
    lua_pushlightuserdata(L, lua_menu_instance_fields);
    lua_setfield(L, -2, "fields");
    lua_pushcfunction(L, luaCB_menu_remove);
    lua_setfield(L, -2, "remove");
    lua_pushstring(L, parent);
    lua_setfield(L, -2, "parent");
    lua_setmetatable(L, -2);
    
    lua_pushvalue(L, 1);
    load_menu_entry(L, new_entry, NULL, "unknown");
    menu_add(parent, new_entry->menu_entry, 1);
    if (!streq(parent, "Scripts"))
    {
        printf("  [i] menu: %s - %s\n", parent, new_entry->menu_entry->name);
    }
    lua_pop(L, 1);
    
    return 1; //return the userdata object
}

/// Set the value of some existing ML menu entry
// @param menu name of the parent menu ('Audio', 'Expo', 'Overlay', 'Shoot', 'Movie', etc)
// @param entry name of the menu entry
// @param value the value to set
// @return whether or not the call was sucessful
// @function set
static int luaCB_menu_set(lua_State * L)
{
    LUA_PARAM_STRING(menu, 1);
    LUA_PARAM_STRING(entry, 2);
    if(lua_isinteger(L, 3))
    {
        LUA_PARAM_INT(value, 3);
        lua_pushboolean(L, menu_set_value_from_script(menu, entry, value));
    }
    else
    {
        LUA_PARAM_STRING(value, 3);
        char * copy = copy_string(value);
        lua_pushboolean(L, menu_set_str_value_from_script(menu, entry, copy, -1));
        free(copy);
    }
    return 1;
}

/// Get the value of some existing ML menu entry
// @param menu name of the parent menu ('Audio', 'Expo', 'Overlay', 'Shoot', 'Movie', etc)
// @param entry name of the menu entry
// @return the value of the menu entry
// @function get
static int luaCB_menu_get(lua_State * L)
{
    LUA_PARAM_STRING(menu, 1);
    LUA_PARAM_STRING(entry, 2);
    lua_pushinteger(L, menu_get_value_from_script(menu, entry));
    return 1;
}

/// Block the ML menu from redrawing (if you wand to do custom drawing)
// @tparam bool enaabled
// @function block
static int luaCB_menu_block(lua_State * L)
{
    LUA_PARAM_BOOL(enabled, 1);
    menu_redraw_blocked = enabled;
    return 0;
}

/// Open ML menu
// @function open
static int luaCB_menu_open(lua_State * L)
{
    gui_open_menu();
    msleep(1000);
    return 0;
}

/// Close ML menu
// @function close
static int luaCB_menu_close(lua_State * L)
{
    gui_stop_menu();
    msleep(1000);
    return 0;
}

static int luaCB_menu_index(lua_State * L)
{
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// Get whether or not the ML menu is visible
    //@tfield bool visible
    if(!strcmp(key, "visible")) lua_pushboolean(L, gui_menu_shown());
    else lua_rawget(L, 1);
    return 1;
}

static int luaCB_menu_newindex(lua_State * L)
{
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    if(!strcmp(key, "visible")) return luaL_error(L, "'%s' is readonly!", key);
    else lua_rawset(L, 1);
    return 0;
}

/// Represents a menu item
// @type menu

static int luaCB_menu_instance_index(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// current value of the menu item
    // @tfield ?int|string value
    if(!strcmp(key, "value"))
    {
        if(script_entry->menu_entry->choices)
        {
            lua_pushstring(L, script_entry->menu_entry->choices[script_entry->menu_value]);
        }
        else
        {
            lua_pushinteger(L, script_entry->menu_value);
        }
    }
    /// Name for the menu item
    // @tfield string name
    else if(!strcmp(key, "name")) lua_pushstring(L, script_entry->menu_entry->name);
    /// Help text for the menu item (line 1)
    // @tfield string help
    else if(!strcmp(key, "help")) lua_pushstring(L, script_entry->menu_entry->help);
    /// Help text for the menu item (line 2)
    // @tfield string help2
    else if(!strcmp(key, "help2")) lua_pushstring(L, script_entry->menu_entry->help2);
    /// Advanced setting in submenus
    // @tfield bool advanced
    else if(!strcmp(key, "advanced")) lua_pushinteger(L, script_entry->menu_entry->advanced);
    /// Dependencies for this menu item.
    // If the dependecies are not met, the item will be greyed out and a warning will appear at the bottom of the screen.
    // @tfield int depends_on @{constants.DEPENDS_ON}
    else if(!strcmp(key, "depends_on")) lua_pushinteger(L, script_entry->menu_entry->depends_on);
    /// Editing mode for the menu item
    // @tfield int edit_mode
    else if(!strcmp(key, "edit_mode")) lua_pushinteger(L, script_entry->menu_entry->edit_mode);
    /// Hidden from main menu
    // @tfield bool hidden
    else if(!strcmp(key, "hidden")) lua_pushboolean(L, script_entry->menu_entry->hidden);
    /// The type of icon to use for this menu item
    // @tfield int icon_type @{constants.ICON_TYPE}
    else if(!strcmp(key, "icon_type")) lua_pushinteger(L, script_entry->menu_entry->icon_type);
    /// Hidden from junkie menu
    // @tfield bool jhidden
    else if(!strcmp(key, "jhidden")) lua_pushboolean(L, script_entry->menu_entry->jhidden);
    /// The maximum value the menu item can have
    // @tfield int max
    else if(!strcmp(key, "max")) lua_pushinteger(L, script_entry->menu_entry->max);
    /// The minimum value the menu item can have
    // @tfield int min
    else if(!strcmp(key, "min")) lua_pushinteger(L, script_entry->menu_entry->min);
    /// Whether or not the menu is selected
    // @tfield int selected
    else if(!strcmp(key, "selected")) lua_pushboolean(L, script_entry->menu_entry->selected);
    /// Special hide, not toggleable by user
    // @tfield bool shidden
    else if(!strcmp(key, "shidden")) lua_pushboolean(L, script_entry->menu_entry->shidden);
    /// Present in "my menu"
    // @tfield bool starred
    else if(!strcmp(key, "starred")) lua_pushboolean(L, script_entry->menu_entry->starred);
    /// Submenu Height
    // @tfield int submenu_height
    else if(!strcmp(key, "submenu_height")) lua_pushinteger(L, script_entry->menu_entry->submenu_height);
    /// Submenu Width
    // @tfield int submenu_width
    else if(!strcmp(key, "submenu_width")) lua_pushinteger(L, script_entry->menu_entry->submenu_width);
    /// The unit for the menu item's value
    // @tfield int unit @{constants.UNIT}
    else if(!strcmp(key, "unit")) lua_pushinteger(L, script_entry->menu_entry->unit);
    /// Suggested operating mode for this menu item
    // @tfield int works_best_in @{constants.DEPENDS_ON}
    else if(!strcmp(key, "works_best_in")) lua_pushinteger(L, script_entry->menu_entry->works_best_in);
    /// Whether or not the backend should run 'select' in it's own task
    // @tfield bool run_in_separate_task
    else if(!strcmp(key, "run_in_separate_task")) lua_pushinteger(L, script_entry->run_in_separate_task);
    /// Function called when menu is toggled
    // @tparam int delta
    // @function select
    else if(!strcmp(key, "select")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->select_ref);
    /// Function called when menu is displayed. Return a string to be displayed.
    // @return string
    // @function update
    else if(!strcmp(key, "update")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->update_ref);
    /// Function called when menu is displayed. Return a string to be displayed in the info area (in green).
    // @return string
    // @function info
    else if(!strcmp(key, "info")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->info_ref);
    /// Function called when menu is displayed. Return a string to be displayed on the right side of the menu item
    // @return string
    // @function rinfo
    else if(!strcmp(key, "rinfo")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->rinfo_ref);
    /// Function called when menu is displayed. Return a string when there is a warning (menu will be greyed out).
    // @return string
    // @function warning
    else if(!strcmp(key, "warning")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->warning_ref);
    else
    {
        //retrieve the key from the metatable
        if(lua_getmetatable(L, 1))
        {
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    return 1;
}

// we can't really maintain const correctness here because of the struct definition, const is discarded to avoid compiler warnings
// we will only ever put dynamically created non-const strings in our structs for ML backends, so it's okay to discard the const
static void set_string(const char ** entry, const char * value)
{
    if(*entry) free((char *)*entry);
    *entry = copy_string(value);
}

static int luaCB_menu_instance_newindex(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    if(!strcmp(key, "value"))
    {
        if(script_entry->menu_entry->choices)
        {
            LUA_PARAM_STRING(value, 3);
            script_entry->menu_value = get_index_for_choices(script_entry->menu_entry, value);
        }
        else
        {
            LUA_PARAM_INT(value, 3);
            script_entry->menu_value = value;
        }
    }
    else if(!strcmp(key, "name")) { LUA_PARAM_STRING(value, 3); set_string(&(script_entry->menu_entry->name),value); }
    else if(!strcmp(key, "help")) { LUA_PARAM_STRING(value, 3); set_string(&(script_entry->menu_entry->help),value); }
    else if(!strcmp(key, "help2")) { LUA_PARAM_STRING(value, 3); set_string(&(script_entry->menu_entry->help2),value); }
    else if(!strcmp(key, "advanced")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->advanced = value; }
    else if(!strcmp(key, "depends_on")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->depends_on = value; }
    else if(!strcmp(key, "edit_mode")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->edit_mode = value; }
    else if(!strcmp(key, "hidden")) { LUA_PARAM_BOOL(value, 3); script_entry->menu_entry->hidden = value; }
    else if(!strcmp(key, "icon_type")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->icon_type = value; }
    else if(!strcmp(key, "jhidden")) { LUA_PARAM_BOOL(value, 3); script_entry->menu_entry->jhidden = value; }
    else if(!strcmp(key, "max")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->max = value; }
    else if(!strcmp(key, "min")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->min = value; }
    else if(!strcmp(key, "selected")) { LUA_PARAM_BOOL(value, 3); script_entry->menu_entry->selected = value; }
    else if(!strcmp(key, "shidden")) { LUA_PARAM_BOOL(value, 3); script_entry->menu_entry->shidden = value; }
    else if(!strcmp(key, "starred")) { LUA_PARAM_BOOL(value, 3); script_entry->menu_entry->starred = value; }
    else if(!strcmp(key, "submenu_height")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->submenu_height = value; }
    else if(!strcmp(key, "submenu_width")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->submenu_width = value; }
    else if(!strcmp(key, "unit")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->unit = value; }
    else if(!strcmp(key, "works_best_in")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->works_best_in = value; }
    else if(!strcmp(key, "run_in_separate_task")) { LUA_PARAM_BOOL(value, 3); script_entry->run_in_separate_task = value; }
    else if(!strcmp(key, "select"))
    {
        if(script_entry->select_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->select_ref);
        if(!lua_isfunction(L, 3))
        {
            script_entry->select_ref = LUA_NOREF;
            script_entry->menu_entry->select = NULL;
        }
        else
        {
            script_entry->select_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            script_entry->menu_entry->select = script_menu_select;
        }
    }
    else if(!strcmp(key, "update"))
    {
        if(script_entry->update_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->update_ref);
        if(lua_isnil(L, 3)) script_entry->update_ref = LUA_NOREF;
        else
        {
            script_entry->update_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }
    else if(!strcmp(key, "info"))
    {
        if(script_entry->info_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->info_ref);
        if(lua_isnil(L, 3)) script_entry->info_ref = LUA_NOREF;
        else
        {
            script_entry->info_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }
    else if(!strcmp(key, "rinfo"))
    {
        if(script_entry->rinfo_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->rinfo_ref);
        if(lua_isnil(L, 3)) script_entry->rinfo_ref = LUA_NOREF;
        else
        {
            script_entry->rinfo_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }
    else if(!strcmp(key, "warning"))
    {
        if(script_entry->warning_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->warning_ref);
        if(lua_isnil(L, 3)) script_entry->warning_ref = LUA_NOREF;
        else
        {
            script_entry->warning_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }
    else
    {
        //set the key in the metatable
        if(lua_getmetatable(L, 1))
        {
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    return 0;
}

static int get_function_ref(lua_State * L, const char * name)
{
    if(lua_getfield(L, -1, name) == LUA_TFUNCTION)
    {
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        lua_pop(L,1);
        return LUA_NOREF;
    }
}

static int get_ref(lua_State * L, const char * name)
{
    if(lua_getfield(L, -1, name))
    {
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        lua_pop(L,1);
        return LUA_NOREF;
    }
}

static void load_menu_entry(lua_State * L, struct script_menu_entry * script_entry, struct menu_entry * menu_entry, const char * default_name)
{
    if(!menu_entry)
    {
        menu_entry = malloc(sizeof(struct menu_entry));
        if(!menu_entry)
        {
            luaL_error(L, "malloc error creating menu_entry");
            return;
        }
    }
    memset(menu_entry, 0, sizeof(struct menu_entry));
    script_entry->L = L;
    script_entry->menu_entry = menu_entry;
    script_entry->run_in_separate_task = LUA_FIELD_BOOL("run_in_separate_task", 0);
    menu_entry->priv = script_entry;
    menu_entry->name = LUA_FIELD_STRING("name", default_name);
    menu_entry->help = LUA_FIELD_STRING("help", "");
    menu_entry->help2 = LUA_FIELD_STRING("help2", "");
    menu_entry->depends_on = LUA_FIELD_INT("depends_on", 0);

    /* menu items with a select function, that don't seem to be a value item or a submenu item,
     * are displayed as actions (ICON_TYPE.ACTION) by default */
    int has_select  = get_function_ref(L, "select") != LUA_NOREF;
    int has_value   = lua_getfield(L, -1, "value")  == LUA_TNUMBER; lua_pop(L, 1);
    int has_min     = lua_getfield(L, -1, "min")    == LUA_TNUMBER; lua_pop(L, 1);
    int has_max     = lua_getfield(L, -1, "max")    == LUA_TNUMBER; lua_pop(L, 1);
    int has_submenu = lua_getfield(L, -1, "submenu") == LUA_TTABLE; lua_pop(L, 1);
    int default_icon_type = !has_select || has_submenu || has_value || has_min || has_max ? 0 : IT_ACTION;
    menu_entry->icon_type = LUA_FIELD_INT("icon_type", default_icon_type);

    menu_entry->unit = LUA_FIELD_INT("unit", 0);
    menu_entry->min = LUA_FIELD_INT("min", 0);
    menu_entry->max = LUA_FIELD_INT("max", 0);
    menu_entry->works_best_in = LUA_FIELD_INT("works_best_in", 0);
    menu_entry->submenu_width = LUA_FIELD_INT("submenu_width", 0);
    menu_entry->submenu_height = LUA_FIELD_INT("submenu_height", 0);
    menu_entry->hidden = LUA_FIELD_BOOL("hidden", 0);
    menu_entry->jhidden = LUA_FIELD_BOOL("jhidden", 0);
    menu_entry->shidden = LUA_FIELD_BOOL("shidden", 0);
    menu_entry->starred = LUA_FIELD_BOOL("starred", 0);
    /// List of strings to display as choices in the menu item
    // @tfield table choices
    if(lua_getfield(L, -1, "choices") == LUA_TTABLE)
    {
        int choices_count = luaL_len(L, -1);
        menu_entry->choices = malloc(sizeof(char*) * choices_count);
        if(menu_entry->choices)
        {
            int choice_index = 0;
            for (choice_index = 0; choice_index < choices_count; choice_index++)
            {
                if(lua_geti(L, -1, choice_index + 1) == LUA_TSTRING) //lua arrays are 1 based
                {
                    menu_entry->choices[choice_index] = lua_tostring(L, -1);
                }
                else
                {
                    fprintf(stderr, "invalid choice[%d]\n", choice_index);
                    menu_entry->choices[choice_index] = NULL;
                    choices_count = choice_index;
                }
                lua_pop(L, 1);
            }
            menu_entry->min = 0;
            menu_entry->max = choices_count - 1;
        }
    }
    lua_pop(L, 1);
    
    script_entry->select_ref = get_ref(L, "select");
    script_entry->update_ref = get_ref(L, "update");
    script_entry->warning_ref = get_ref(L, "warning");
    script_entry->info_ref = get_ref(L, "info");
    script_entry->rinfo_ref = get_ref(L, "rinfo");

    if(script_entry->select_ref != LUA_NOREF)
    {
        /* optionally, a menu entry can have a select function */
        menu_entry->select = script_menu_select;
    }

    /* all menu entries have an update function */
    menu_entry->update = script_menu_update;

    /// Table of more menu tables that define a submenu
    // @tfield table submenu
    if(lua_getfield(L, -1, "submenu") == LUA_TTABLE)
    {
        int submenu_count = luaL_len(L, -1);
        if(submenu_count > 0)
        {
            int submenu_index = 0;
            script_entry->menu_value = 1;
            script_entry->menu_entry->icon_type = IT_SUBMENU;
            script_entry->menu_entry->select = menu_open_submenu;
            script_entry->menu_entry->children = malloc(sizeof(struct menu_entry) * (1 + submenu_count));
            memset(script_entry->menu_entry->children, 0, sizeof(struct menu_entry) * (1 + submenu_count));
            
            if(lua_getmetatable(L, -3))
            {
                //create a new submenu table
                lua_newtable(L);
                lua_setfield(L, -2, "submenu");
                lua_pop(L, 1);
            }
            else
            {
                fprintf(stderr, "warning: could not create metatable submenu");
            }
            
            for (submenu_index = 0; submenu_index < submenu_count; submenu_index++)
            {
                if(lua_geti(L, -1, submenu_index + 1) == LUA_TTABLE) //lua arrays are 1 based
                {
                    struct script_menu_entry * new_entry = lua_newuserdata(L, sizeof(struct script_menu_entry));
                    lua_pushvalue(L, -1);
                    new_entry->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                    
                    //add a metatable to the userdata object for value lookups and to store submenu
                    lua_newtable(L);
                    lua_pushcfunction(L, luaCB_menu_instance_index);
                    lua_setfield(L, -2, "__index");
                    lua_pushcfunction(L, luaCB_menu_instance_newindex);
                    lua_setfield(L, -2, "__newindex");
                    lua_setmetatable(L, -2);
                    
                    lua_pushvalue(L, -2);
                    load_menu_entry(L, new_entry, &(script_entry->menu_entry->children[submenu_index]), "unknown");
                    lua_pop(L, 1);
                    
                    //add the new userdata object to the submenu table of the parent metatable, using the menu name as a key
                    if(lua_getmetatable(L, -5))
                    {
                        if(lua_getfield(L, -1, "submenu") == LUA_TTABLE)
                        {
                            lua_pushvalue(L, -3);
                            lua_setfield(L, -2, script_entry->menu_entry->children[submenu_index].name);
                        }
                        else
                        {
                            fprintf(stderr, "warning: could not get metatable submenu");
                        }
                        lua_pop(L, 2);
                    }
                    else
                    {
                        fprintf(stderr, "warning: could not get parent metatable");
                    }
                    
                    lua_pop(L, 1);//userdata
                }
                else
                {
                    fprintf(stderr, "invalid submenu[%d]\n", submenu_index);
                }
                lua_pop(L, 1);
            }
            script_entry->menu_entry->children[submenu_index].priv = MENU_EOL_PRIV;
        }
    }
    lua_pop(L, 1);
    
    //load default 'value' so our index metamethod works
    if(menu_entry->choices)
    {
        const char * str_value = LUA_FIELD_STRING("value", "");
        script_entry->menu_value = get_index_for_choices(menu_entry, str_value);
    }
    else
    {
        script_entry->menu_value = LUA_FIELD_INT("value", has_submenu ? 1 : 0);
    }
    
}

/// Removes this menu entry
// @function remove
static int luaCB_menu_remove(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    if(lua_getmetatable(L, 1))
    {
        const char * parent = LUA_FIELD_STRING("parent", "Scripts");
        menu_remove(parent, script_entry->menu_entry, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, script_entry->self_ref);
        if(script_entry->menu_entry->name) free((char*)script_entry->menu_entry->name);
        script_entry->menu_entry->name = NULL;
        if(script_entry->menu_entry->help) free((char*)script_entry->menu_entry->help);
        script_entry->menu_entry->help = NULL;
        if(script_entry->menu_entry->help2) free((char*)script_entry->menu_entry->help2);
        script_entry->menu_entry->help2 = NULL;
    }
    else
    {
        return luaL_error(L, "could not get metatable for userdata");
    }
    return 0;
}

static const char * lua_menu_fields[] =
{
    "visible",
    NULL
};

const luaL_Reg menulib[] =
{
    {"new", luaCB_menu_new},
    {"set", luaCB_menu_set},
    {"get", luaCB_menu_get},
    {"open", luaCB_menu_open},
    {"close", luaCB_menu_close},
    {"block", luaCB_menu_block},
    {NULL, NULL}
};

LUA_LIB(menu)
