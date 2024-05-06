
#ifndef __MX_JSON_H_
#define __MX_JSON_H_

#include <stdbool.h>

#include <json-c/json.h>


// Make sure it is not covered by original json_type enum
typedef enum json_type_ext {
    json_type_any = -2,
    json_type_unknown = -1,
}json_type_ext;


struct json_object* json_new(void);
struct json_object* json_copy(struct json_object *obj);
struct json_object* json_delete(struct json_object *obj);

struct json_object* json_get(struct json_object *obj, const char *name, int type);
struct json_object* json_get_verbose(struct json_object *obj, const char *name, int type, const char *file, unsigned int line);
#define json_getv(obj, name, type) json_get_verbose(obj, name, type, __FILENAME__, __LINE__)





#define JSON_COMPARE_FLAG_NON_STRICT         (0)
#define JSON_COMPARE_FLAG_STRICT_OBJECTS     (0x1<<0)
#define JSON_COMPARE_FLAG_STRICT_ARRAYS      (0x1<<1)
#define JSON_COMPARE_FLAG_STRICT             (JSON_COMPARE_FLAG_STRICT_OBJECTS | JSON_COMPARE_FLAG_STRICT_ARRAYS)

bool json_compare(struct json_object *obj, struct json_object *pattern, const unsigned int flags);



#endif /* __MX_JSON_H_ */
