
#include "mx/json.h"
#include "mx/log.h"
#include "mx/misc.h"

#include <string.h>





struct json_object* json_new(void)
{
    json_object *ret = json_object_new_object();
    if (!ret)
        RESET("Out of memory");
    return ret;
}


struct json_object* json_copy(struct json_object *obj)
{
    struct json_object *ret = json_new();
    json_object_object_foreach(obj, key, val) {
        json_object_object_add(ret, key, json_object_get(val));
    }
    return ret;
}


struct json_object* json_delete(struct json_object *obj)
{
    if (obj)
        json_object_put(obj);
    return NULL;
}



struct json_object* json_get(struct json_object *obj, const char *name, int type)
{
    json_object *ret;
    if (!json_object_object_get_ex(obj, name, &ret))
        return NULL;

    if (type != json_type_any) {
        if (!json_object_is_type(ret, type))
            return NULL;
    }

    return ret;
}


struct json_object* json_get_verbose(struct json_object *obj, const char *name, int type, const char *file, unsigned int line)
{
    struct json_object *ret;
    if (!json_object_object_get_ex(obj, name, &ret)) {
        log_error(deflogger, file, line, "Json key '%s' not found", name);
        return NULL;
    }

    if (type != json_type_any) {
        if (!json_object_is_type(ret, type)) {
            log_error(deflogger, file, line, "Json field '%s' has wrong type", name);
            return NULL;
        }
    }

    return ret;
}





static bool json_compare_string(struct json_object *obj, struct json_object *pattern)
{
    if (json_object_get_string_len(pattern) != json_object_get_string_len(obj))
        return false;

    if (strcmp(json_object_get_string(pattern), json_object_get_string(obj)) != 0)
        return false;

    return true;
}


static bool json_compare_array(struct json_object *obj, struct json_object *pattern, const unsigned int flags)
{
    size_t obj_length = json_object_array_length(obj);
    size_t pat_length = json_object_array_length(pattern);

    if (flags & JSON_COMPARE_FLAG_STRICT_ARRAYS) {
        if (obj_length != pat_length)
            return false;

        for (size_t idx=0; idx<pat_length; idx++) {
            if (!json_compare(json_object_array_get_idx(obj, idx), json_object_array_get_idx(pattern, idx), flags))
                return false;
        }

        return true;
    }

    // just make sure that all pattern objects are available in 'obj' json
    // order is not important
    for (size_t pat_idx=0; pat_idx<pat_length; pat_idx++) {
        bool found = false;
        for (size_t obj_idx=0; obj_idx<obj_length; obj_idx++) {
            if (json_compare(json_object_array_get_idx(obj, obj_idx),
                             json_object_array_get_idx(pattern, pat_idx),
                             flags)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    return true;
}


static bool json_compare_object(struct json_object *obj, struct json_object *pattern, const unsigned int flags)
{
    if (flags & JSON_COMPARE_FLAG_STRICT_OBJECTS) {
        // For strict checking both objects need to have the same length
        if (json_object_object_length(obj) != json_object_object_length(pattern))
            return false;
    }

    // Compare every pattern's key, value from
    {
        json_object *objval;
        json_object_object_foreach(pattern, key, patval) {
            if (!json_object_object_get_ex(obj, key, &objval))
                return false;

            if (!json_compare(objval, patval, flags))
                return false;
        }
    }

    // If strict objects is selected the 'obj' json has to contain only 'pattern' keys
    if (flags & JSON_COMPARE_FLAG_STRICT_OBJECTS) {
        json_object *patval;
        json_object_object_foreach(obj, key, objval) {
            UNUSED(objval);
            if (!json_object_object_get_ex(pattern, key, &patval))
                return false;   // key not available in pattern
        }
    }

    return true;
}


bool json_compare(struct json_object *obj, struct json_object *pattern, const unsigned int flags)
{
    if (json_object_get_type(pattern) != json_object_get_type(obj))
        return false;

    switch (json_object_get_type(pattern)) {
    case json_type_object:
        return json_compare_object(obj, pattern, flags);

    case json_type_array:
        return json_compare_array(obj, pattern, flags);

    case json_type_string:
        return json_compare_string(obj, pattern);

    case json_type_boolean:
        return (json_object_get_boolean(obj) == json_object_get_boolean(pattern)) ? true : false;

    case json_type_int:
        return (json_object_get_int(obj) == json_object_get_int(pattern)) ? true : false;

    case json_type_double:
        return (json_object_get_double(obj) == json_object_get_double(pattern)) ? true : false;

    case json_type_null:
        return true;

    default:
        break;
    }

    return false;
}
