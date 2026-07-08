#ifndef PAYLOADS_INTERNAL_H
#define PAYLOADS_INTERNAL_H

#include "payloads.h"

#if PLD_HAVE_EXPORT
#include <stdio.h>  /* asprintf() */
#include <malloc.h> /* free() */
#include <cJSON.h>

/** Export struct data to JSON (does not allocate the container) */
typedef pld_error_t (*fn_data_to_json_t)(const void *data, enum pld_export_json_style style, cJSON* output);

/** Export struct to JSON (allocates the container and contains name and descr, if needed for export style) */
typedef pld_error_t (*fn_export_to_json_t)(const void *data, enum pld_export_json_style style, cJSON** output);

/**
 * Helper macro used in the auto-generated export functions: jump to label "cjfail" if the argument is NULL.
 */
#define CJSON_TRY(x) do { \
    if (NULL == (x)) { \
        pld_log_error("JSON error: " _pld_str(x)); \
        goto cjfail; \
    } \
} while (0)

/**
 * Same as CJSON_TRY, but the additional argument is cleaned using `free` (use for non-JSON args) on error
 */
#define CJSON_TRY_F(x, tmp) do { \
    if (NULL == (x)) { \
        pld_log_error("JSON error: " _pld_str(x)); \
        free((tmp)); \
        goto cjfail; \
    } \
} while (0)

#endif /* PLD_HAVE_EXPORT */

/**
 * Payload recognition function typedef (checks constant fields)
 */
typedef bool (*fn_identify_t)(const uint8_t *payload);

#if PLD_HAVE_ALLOC
/**
 * Parsing function typedef
 */
typedef pld_error_t (*fn_parse_t)(const uint8_t *payload, size_t length, pld_alloc_fn alloc, void **output);
#endif /* PLD_HAVE_ALLOC */

/**
 * Parsing function typedef (static version)
 */
typedef pld_error_t (*fn_parse_static_t)(const uint8_t *payload, size_t length, void *output);

#if PLD_HAVE_ALLOC
/**
 * Building function typedef
 */
typedef pld_error_t (*fn_build_t)(const void * const data, pld_alloc_fn alloc, uint8_t **output, size_t *len);
#endif /* PLD_HAVE_ALLOC */

/**
 * Building function typedef (static version)
 */
typedef pld_error_t (*fn_build_static_t)(const void * const data, uint8_t *output, size_t capacity);

/**
 * Entry for the payload look-up tables
 */
struct pld_spec {
    /** Payload ID, used for invalid pointer detection */
    uint16_t pld_id;
    /** Payload name */
    const char *name;
    /** Address for recognizer matching */
    struct pld_address address;
    /** Payload binary size - or minimum, if growable */
    size_t bin_size;
    /** Payload C size (sizeof) - or minimum, if growable */
    size_t c_size;
    /** True if the payload has a tail array */
    bool growable;
    /** Tail element binary size */
    size_t tail_elem_bin_size;
    /** Tail element C size */
    size_t tail_elem_c_size;
    /** Include in payload recognition */
    bool recognize;
    /** Recognizer test function (checks constant fields) */
    fn_identify_t tester;
    /** Parser function (static) */
    fn_parse_static_t parse_static;
    /** Builder function (static) */
    fn_build_static_t build_static;
#if PLD_HAVE_ALLOC
    /** Parser function */
    fn_parse_t parse;

    /** Builder function */
    fn_build_t build;
#endif /* PLD_HAVE_ALLOC */
#if PLD_HAVE_EXPORT
    /** Payload description, included in the JSON dump */
    const char *descr;
    /** JSON export function (internal) */
    fn_data_to_json_t data_to_json;
#endif /* PLD_HAVE_EXPORT */
};

/** Helper union for transmuting u32 to float and back */
union x32 {
    /** as unsigned */
    uint32_t u;
    /** as signed */
    int32_t i;
    /** as float */
    float f;
};
typedef union x32 x32_t;

/** Helper union for transmuting u64 to double and back */
union x64 {
    /** as unsigned */
    uint64_t u;
    /** as signed */
    int64_t i;
    /** as float (double) */
    double f;
};
typedef union x64 x64_t;

/** Helper union for transmuting u16 to i16 and back */
union x16 {
    /** as unsigned */
    uint16_t u;
    /** as signed */
    int16_t i;
};
typedef union x16 x16_t;

/** Helper union for transmuting u8 to i8 and back */
union x8 {
    /** as unsigned */
    uint8_t u;
    /** as signed */
    int8_t i;
};
typedef union x8 x8_t;

/**
 * Find payload spec by the ID number in a payloads module.
 *
 * @param[in] module - a pointer to a module struct
 * @param[in] kind - payload ID
 * @param[out] result - the result will be output here
 * @return success
 */
pld_error_t pld_find_spec_by_kind_in_module(const pld_module_t *module, uint32_t kind, const struct pld_spec ** result);

/**
 * Find payload spec by the ID number in a NULL-terminated array of payload modules.
 * @param[in] modules - array of modules, with NULL as the last element
 * @param[in] kind - payload ID
 * @param[out] result - the result will be output here
 * @return success
 */
pld_error_t pld_find_spec_by_kind(const pld_module_t **modules, uint32_t kind, const struct pld_spec ** result);

#if PLD_HAVE_EXPORT
/**
 * Export a payload to JSON.
 *
 * Use `cJSON_Delete(json)` to deallocate it.
 *
 * @param spec - payload spec (usually retrieved by `pld_find_spec_by_kind`)
 * @param pld_struct - payload to export
 * @param style - export style
 * @param output - the JSON will be output here
 * @return success
 */
pld_error_t pld_to_json_by_spec(
    const struct pld_spec *spec,
    const void *pld_struct,
    enum pld_export_json_style style,
    cJSON** output
);
#endif /* PLD_HAVE_EXPORT */

#endif /* PAYLOADS_INTERNAL_H */
