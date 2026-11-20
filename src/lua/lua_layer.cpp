#include <lua.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "app/context.h"
#include "gfx/layer.h"
#include "lua/registration.h"

namespace flux::lua {

namespace {

flux::ui::render_layer check_layer(lua_State* L, int index) {
    const char* name = luaL_checkstring(L, index);
    flux::ui::render_layer layer = flux::ui::render_layer::content;
    if (!flux::ui::parse_render_layer(name, layer)) {
        luaL_error(L, "unknown layer '%s'", name);
    }
    return layer;
}

float read_number_field(lua_State* L, int table, const char* key, float fallback) {
    lua_getfield(L, table, key);
    float value = fallback;
    if (lua_isnumber(L, -1)) {
        value = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);
    return value;
}

void read_pair(lua_State* L, int table, float& x, float& y) {
    if (!lua_istable(L, table)) {
        return;
    }
    bool has_x = false;
    bool has_y = false;
    lua_rawgeti(L, table, 1);
    if (lua_isnumber(L, -1)) {
        x = static_cast<float>(lua_tonumber(L, -1));
        has_x = true;
    }
    lua_pop(L, 1);
    lua_rawgeti(L, table, 2);
    if (lua_isnumber(L, -1)) {
        y = static_cast<float>(lua_tonumber(L, -1));
        has_y = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, table, "x");
    if (lua_isnumber(L, -1)) {
        x = static_cast<float>(lua_tonumber(L, -1));
        has_x = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, table, "y");
    if (lua_isnumber(L, -1)) {
        y = static_cast<float>(lua_tonumber(L, -1));
        has_y = true;
    }
    lua_pop(L, 1);
    if (has_x && !has_y) {
        y = x;
    }
}

bool read_value(lua_State* L, int index, flux::gfx::layer_property property, flux::gfx::layer_value& value) {
    if (lua_isnumber(L, index)) {
        const float number = static_cast<float>(lua_tonumber(L, index));
        value.x = number;
        value.y = property == flux::gfx::layer_property::scale ? number : 0.0f;
        value.z = 0.0f;
        value.dims = property == flux::gfx::layer_property::offset ? 3
            : (property == flux::gfx::layer_property::scale ? 2 : 1);
        return true;
    }
    if (!lua_istable(L, index)) {
        return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool any = false;
    bool has_y = false;
    lua_rawgeti(L, index, 1);
    if (lua_isnumber(L, -1)) {
        x = static_cast<float>(lua_tonumber(L, -1));
        any = true;
    }
    lua_pop(L, 1);
    lua_rawgeti(L, index, 2);
    if (lua_isnumber(L, -1)) {
        y = static_cast<float>(lua_tonumber(L, -1));
        any = true;
        has_y = true;
    }
    lua_pop(L, 1);
    lua_rawgeti(L, index, 3);
    if (lua_isnumber(L, -1)) {
        z = static_cast<float>(lua_tonumber(L, -1));
        any = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "x");
    if (lua_isnumber(L, -1)) {
        x = static_cast<float>(lua_tonumber(L, -1));
        any = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "y");
    if (lua_isnumber(L, -1)) {
        y = static_cast<float>(lua_tonumber(L, -1));
        any = true;
        has_y = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "z");
    if (lua_isnumber(L, -1)) {
        z = static_cast<float>(lua_tonumber(L, -1));
        any = true;
    }
    lua_pop(L, 1);
    if (!any) {
        return false;
    }
    if (property == flux::gfx::layer_property::scale && !has_y) {
        y = x;
    }
    value.x = x;
    value.y = y;
    value.z = z;
    value.dims = property == flux::gfx::layer_property::offset ? 3
        : (property == flux::gfx::layer_property::scale ? 2 : 1);
    return true;
}

void set_easing(flux::gfx::layer_animation_track& track, const char* name) {
    if (name == nullptr || std::strcmp(name, "out-cubic") == 0
        || std::strcmp(name, "ease-out") == 0) {
        track.linear = false;
        track.easing_x1 = 0.33f;
        track.easing_y1 = 1.0f;
        track.easing_x2 = 0.68f;
        track.easing_y2 = 1.0f;
    } else if (std::strcmp(name, "linear") == 0) {
        track.linear = true;
    } else if (std::strcmp(name, "in-cubic") == 0
               || std::strcmp(name, "ease-in") == 0) {
        track.linear = false;
        track.easing_x1 = 0.32f;
        track.easing_y1 = 0.0f;
        track.easing_x2 = 0.67f;
        track.easing_y2 = 0.0f;
    } else if (std::strcmp(name, "in-out-cubic") == 0
               || std::strcmp(name, "ease-in-out") == 0) {
        track.linear = false;
        track.easing_x1 = 0.65f;
        track.easing_y1 = 0.0f;
        track.easing_x2 = 0.35f;
        track.easing_y2 = 1.0f;
    } else {
        track.linear = false;
        track.easing_x1 = 0.25f;
        track.easing_y1 = 0.1f;
        track.easing_x2 = 0.25f;
        track.easing_y2 = 1.0f;
    }
}

void read_easing(lua_State* L, int index, flux::gfx::layer_animation_track& track) {
    if (lua_isstring(L, index)) {
        set_easing(track, lua_tostring(L, index));
        return;
    }
    if (lua_istable(L, index)) {
        track.linear = false;
        track.easing_x1 = read_number_field(L, index, "x1", 0.25f);
        track.easing_y1 = read_number_field(L, index, "y1", 0.1f);
        track.easing_x2 = read_number_field(L, index, "x2", 0.25f);
        track.easing_y2 = read_number_field(L, index, "y2", 1.0f);
        return;
    }
    set_easing(track, "out-cubic");
}

flux::gfx::layer_value default_from(const flux::gfx::layer_transform& base, flux::gfx::layer_property property) {
    flux::gfx::layer_value value;
    value.dims = property == flux::gfx::layer_property::offset ? 3
        : (property == flux::gfx::layer_property::scale ? 2 : 1);
    switch (property) {
    case flux::gfx::layer_property::opacity:
        value.x = base.opacity;
        break;
    case flux::gfx::layer_property::offset:
        value.x = base.offset_x;
        value.y = base.offset_y;
        break;
    case flux::gfx::layer_property::scale:
        value.x = base.scale_x;
        value.y = base.scale_y;
        break;
    case flux::gfx::layer_property::rotation:
        value.x = base.rotation;
        break;
    }
    return value;
}

bool read_track(lua_State* L, int index, flux::gfx::layer_property property, const flux::gfx::layer_transform& base,
                flux::gfx::layer_animation_track& track) {
    luaL_checktype(L, index, LUA_TTABLE);
    track.property = property;
    lua_getfield(L, index, "duration");
    if (lua_isnumber(L, -1)) {
        track.duration = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "delay");
    if (lua_isnumber(L, -1)) {
        track.delay = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "iterations");
    if (lua_isnumber(L, -1)) {
        track.iterations = static_cast<int>(lua_tointeger(L, -1));
    } else if (lua_isstring(L, -1)) {
        const char* value = lua_tostring(L, -1);
        if (std::strcmp(value, "infinite") == 0 || std::strcmp(value, "forever") == 0) {
            track.iterations = 0;
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "direction");
    if (lua_isstring(L, -1)) {
        const char* value = lua_tostring(L, -1);
        if (std::strcmp(value, "reverse") == 0) {
            track.direction = 1;
        } else if (std::strcmp(value, "alternate") == 0) {
            track.direction = 2;
        } else if (std::strcmp(value, "alternate_reverse") == 0) {
            track.direction = 3;
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "easing");
    read_easing(L, lua_gettop(L), track);
    lua_pop(L, 1);

    lua_getfield(L, index, "spring");
    if (lua_istable(L, -1)) {
        track.spring = true;
        track.damping_ratio = read_number_field(L, lua_gettop(L), "damping", 0.7f);
        track.period = read_number_field(L, lua_gettop(L), "period", 0.35f);
        track.initial_velocity = read_number_field(L, lua_gettop(L), "velocity", 0.0f);
    } else if (lua_isboolean(L, -1)) {
        track.spring = lua_toboolean(L, -1) != 0;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "keyframes");
    if (lua_istable(L, -1)) {
        const int count = static_cast<int>(lua_rawlen(L, -1));
        if (count < 2) {
            luaL_error(L, "keyframes needs at least two entries");
        }
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L, -1, i);
            flux::gfx::layer_keyframe key;
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "t");
                if (lua_isnumber(L, -1)) {
                    key.t = static_cast<float>(lua_tonumber(L, -1));
                } else {
                    key.t = count > 1 ? static_cast<float>(i - 1) / (count - 1) : 0.0f;
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "value");
                if (!read_value(L, lua_gettop(L), property, key.value)) {
                    lua_pop(L, 1);
                    lua_pop(L, 1);
                    luaL_error(L, "keyframe value is invalid");
                }
                lua_pop(L, 1);
            } else if (read_value(L, lua_gettop(L), property, key.value)) {
                key.t = count > 1 ? static_cast<float>(i - 1) / (count - 1) : 0.0f;
            } else {
                lua_pop(L, 1);
                lua_pop(L, 1);
                luaL_error(L, "keyframe is invalid");
            }
            lua_pop(L, 1);
            track.keyframes.push_back(key);
        }
        std::sort(track.keyframes.begin(), track.keyframes.end(),
                  [](const flux::gfx::layer_keyframe& a, const flux::gfx::layer_keyframe& b) {
                      return a.t < b.t;
                  });
        if (track.keyframes.front().t > 0.0f) {
            flux::gfx::layer_keyframe first = track.keyframes.front();
            first.t = 0.0f;
            track.keyframes.insert(track.keyframes.begin(), first);
        }
        if (track.keyframes.back().t < 1.0f) {
            flux::gfx::layer_keyframe last = track.keyframes.back();
            last.t = 1.0f;
            track.keyframes.push_back(last);
        }
    } else {
        flux::gfx::layer_value from = default_from(base, property);
        lua_getfield(L, index, "from");
        if (!lua_isnil(L, -1) && !read_value(L, lua_gettop(L), property, from)) {
            lua_pop(L, 1);
            luaL_error(L, "animation 'from' is invalid");
        }
        lua_pop(L, 1);

        flux::gfx::layer_value to = from;
        lua_getfield(L, index, "to");
        if (!read_value(L, lua_gettop(L), property, to)) {
            lua_pop(L, 1);
            luaL_error(L, "animation 'to' is required and must be valid");
        }
        lua_pop(L, 1);

        flux::gfx::layer_keyframe first;
        first.t = 0.0f;
        first.value = from;
        flux::gfx::layer_keyframe last;
        last.t = 1.0f;
        last.value = to;
        track.keyframes.push_back(first);
        track.keyframes.push_back(last);
    }
    lua_pop(L, 1);

    return true;
}

void read_animation(lua_State* L, int index, flux::ui::render_layer layer, flux::gfx::layer_animation_spec& spec) {
    luaL_checktype(L, index, LUA_TTABLE);
    const flux::gfx::layer_transform& base = flux::app::current().renderer.layer_transform(layer);
    static const flux::gfx::layer_property properties[] = {
        flux::gfx::layer_property::opacity,
        flux::gfx::layer_property::offset,
        flux::gfx::layer_property::scale,
        flux::gfx::layer_property::rotation,
    };
    for (flux::gfx::layer_property property : properties) {
        const char* name = flux::gfx::layer_property_name(property);
        lua_getfield(L, index, name);
        if (!lua_isnil(L, -1)) {
            flux::gfx::layer_animation_track track;
            read_track(L, lua_gettop(L), property, base, track);
            spec.tracks.push_back(track);
        }
        lua_pop(L, 1);
    }
    if (spec.tracks.empty()) {
        luaL_error(L, "animation needs at least one property");
    }
}

int l_layer_use(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    if (!flux::app::current().draw.use_layer(layer)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "cannot switch layer");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_reset(lua_State* L) {
    (void)L;
    if (!flux::app::current().draw.use_layer(flux::ui::render_layer::content)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "cannot switch layer");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_push(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    if (!flux::app::current().draw.push_layer(layer)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "cannot push layer");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_pop(lua_State* L) {
    (void)L;
    if (!flux::app::current().draw.pop_layer()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "layer stack is empty");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_active(lua_State* L) {
    lua_pushstring(L, flux::ui::render_layer_name(flux::app::current().draw.active_layer()));
    return 1;
}

int l_layer_ensure(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    std::string error;
    if (!flux::app::current().renderer.ensure_layer(layer, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_exists(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    lua_pushboolean(L, flux::app::current().renderer.layer_exists(layer) ? 1 : 0);
    return 1;
}

int l_layer_list(lua_State* L) {
    lua_createtable(L, flux::ui::k_render_layer_count, 0);
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        lua_pushstring(L, flux::ui::render_layer_name(static_cast<flux::ui::render_layer>(i)));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int l_layer_set(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    flux::gfx::composition_renderer& renderer = flux::app::current().renderer;
    flux::gfx::layer_transform transform = renderer.layer_transform(layer);

    lua_getfield(L, 2, "opacity");
    if (lua_isnumber(L, -1)) {
        transform.opacity = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "offset");
    read_pair(L, lua_gettop(L), transform.offset_x, transform.offset_y);
    lua_pop(L, 1);

    lua_getfield(L, 2, "scale");
    if (lua_isnumber(L, -1)) {
        const float value = static_cast<float>(lua_tonumber(L, -1));
        transform.scale_x = value;
        transform.scale_y = value;
    } else {
        read_pair(L, lua_gettop(L), transform.scale_x, transform.scale_y);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "rotation");
    if (lua_isnumber(L, -1)) {
        transform.rotation = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    std::string error;
    if (!renderer.set_layer_transform(layer, transform, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_get(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    flux::gfx::composition_renderer& renderer = flux::app::current().renderer;
    const flux::gfx::layer_transform& transform = renderer.layer_transform(layer);
    const float scale = renderer.dpi_scale();

    lua_createtable(L, 0, 8);
    lua_pushboolean(L, renderer.layer_exists(layer) ? 1 : 0);
    lua_setfield(L, -2, "exists");
    lua_pushnumber(L, transform.opacity);
    lua_setfield(L, -2, "opacity");
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, transform.offset_x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, transform.offset_y);
    lua_setfield(L, -2, "y");
    lua_setfield(L, -2, "offset");
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, transform.offset_x * scale);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, transform.offset_y * scale);
    lua_setfield(L, -2, "y");
    lua_setfield(L, -2, "pixel_offset");
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, transform.scale_x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, transform.scale_y);
    lua_setfield(L, -2, "y");
    lua_setfield(L, -2, "scale");
    lua_pushnumber(L, transform.rotation);
    lua_setfield(L, -2, "rotation");
    lua_pushnumber(L, renderer.pixel_width() / (scale > 0.0f ? scale : 1.0f));
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, renderer.pixel_height() / (scale > 0.0f ? scale : 1.0f));
    lua_setfield(L, -2, "height");
    return 1;
}

int l_layer_blur(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    if (lua_isnoneornil(L, 2)) {
        lua_pushnumber(L, flux::app::current().renderer.layer_blur(layer));
        return 1;
    }
    const float blur = static_cast<float>(luaL_checknumber(L, 2));
    std::string error;
    if (!flux::app::current().renderer.set_layer_blur(layer, blur, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_shadow(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    if (lua_isnone(L, 2)) {
        lua_pushboolean(L, flux::app::current().renderer.layer_has_shadow(layer) ? 1 : 0);
        return 1;
    }
    if (lua_isnil(L, 2)) {
        lua_pushboolean(L, flux::app::current().renderer.clear_layer_shadow(layer) ? 1 : 0);
        return 1;
    }
    luaL_checktype(L, 2, LUA_TTABLE);

    flux::gfx::layer_shadow shadow;
    shadow.blur = read_number_field(L, 2, "blur", shadow.blur);
    shadow.opacity = read_number_field(L, 2, "opacity", shadow.opacity);
    lua_getfield(L, 2, "offset");
    read_pair(L, lua_gettop(L), shadow.offset_x, shadow.offset_y);
    lua_pop(L, 1);
    lua_getfield(L, 2, "color");
    if (lua_isinteger(L, -1)) {
        shadow.color = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    std::string error;
    if (!flux::app::current().renderer.set_layer_shadow(layer, shadow, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_clip(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    if (lua_isnone(L, 2)) {
        lua_pushboolean(L, flux::app::current().renderer.layer_has_clip(layer) ? 1 : 0);
        return 1;
    }
    if (lua_isnil(L, 2)) {
        lua_pushboolean(L, flux::app::current().renderer.clear_layer_clip(layer) ? 1 : 0);
        return 1;
    }
    luaL_checktype(L, 2, LUA_TTABLE);

    flux::gfx::layer_clip clip;
    clip.x = read_number_field(L, 2, "x", 0.0f);
    clip.y = read_number_field(L, 2, "y", 0.0f);
    clip.w = read_number_field(L, 2, "w", 0.0f);
    clip.h = read_number_field(L, 2, "h", 0.0f);
    clip.radius = read_number_field(L, 2, "radius", 0.0f);

    std::string error;
    if (!flux::app::current().renderer.set_layer_clip(layer, clip, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_layer_animate(lua_State* L) {
    const flux::ui::render_layer layer = check_layer(L, 1);
    flux::gfx::layer_animation_spec spec;
    read_animation(L, 2, layer, spec);

    std::string error;
    const uint64_t id = flux::app::current().renderer.start_animation(layer, spec, error);
    if (id == 0) {
        lua_pushnil(L);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

}

void push_layer_table(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "use", l_layer_use },
        { "reset", l_layer_reset },
        { "push", l_layer_push },
        { "pop", l_layer_pop },
        { "active", l_layer_active },
        { "ensure", l_layer_ensure },
        { "exists", l_layer_exists },
        { "list", l_layer_list },
        { "set", l_layer_set },
        { "get", l_layer_get },
        { "blur", l_layer_blur },
        { "shadow", l_layer_shadow },
        { "clip", l_layer_clip },
        { "animate", l_layer_animate },
        { nullptr, nullptr },
    };
    register_table(L, "layer", functions);
}

}
