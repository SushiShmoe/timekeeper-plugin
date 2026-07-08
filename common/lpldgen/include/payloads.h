#ifndef PAYLOADS_H
#define PAYLOADS_H

#include "conf_payloads.h"

#if PLD_HAVE_EXPORT
/* for asprintf */
#define _GNU_SOURCE
#endif /* PLD_HAVE_EXPORT */

#include <stdint.h>  // int types
#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdarg.h>
#include <math.h>

#if PLD_HAVE_EXPORT
    #include <stdio.h>
    #include <cJSON.h>
#endif

#if PLD_HAVE_LOGGING
    enum pld_log_level {
        PLD_LOG_TRACE = 0,
        PLD_LOG_DEBUG = 1,
        PLD_LOG_INFO = 2,
        PLD_LOG_WARN = 3,
        PLD_LOG_ERROR = 4,
    };

    extern
        __attribute__((weak, format (printf, 2, 3)))
    void pld_log_write(enum pld_log_level level, const char *format, ...);

    #define _pld_str2(a) #a
    #define _pld_str(a) _pld_str2(a)

    #define _pld_log_write_do(_level, _format, ...) do { \
        if (pld_log_write) { \
            pld_log_write(_level, __FILE__ ":" _pld_str(__LINE__) ": " _format, ##__VA_ARGS__); \
        } \
    } while(0)

    #define pld_log_error(_format, ...) _pld_log_write_do(PLD_LOG_ERROR, _format, ##__VA_ARGS__)
    #define pld_log_warn(_format, ...) _pld_log_write_do(PLD_LOG_WARN, _format, ##__VA_ARGS__)
    #define pld_log_info(_format, ...) _pld_log_write_do(PLD_LOG_INFO, _format, ##__VA_ARGS__)
    #define pld_log_debug(_format, ...) _pld_log_write_do(PLD_LOG_DEBUG, _format, ##__VA_ARGS__)
    #define pld_log_trace(_format, ...) _pld_log_write_do(PLD_LOG_TRACE, _format, ##__VA_ARGS__)
#else
    // no-op impls
    #define pld_log_error(msg, ...)
    #define pld_log_warn(msg, ...)
    #define pld_log_info(msg, ...)
    #define pld_log_debug(msg, ...)
    #define pld_log_trace(msg, ...)
#endif

/**
 * Errors returned from libpayload functions
 */
enum pld_error {
    PLD_OK = 0,              //!< Success
    PLD_ERR = 1,             //!< Further unspecified error
    PLD_ERR_ALLOC,           //!< Allocation failed
    PLD_ERR_BAD_ARGS,        //!< Invalid arguments given
    PLD_ERR_NOT_IMPLEMENTED, //!< Not implemented
    PLD_ERR_UNKNOWN_PAYLOAD, //!< Payload not recognized
    PLD_ERR_LENGTH,          //!< Bad buffer length
    PLD_ERR_TAIL_LENGTH,     //!< Bad tail array length
    PLD_ERR_ID_MISMATCH,     //!< Payload ID mismatch
    PLD_ERR_MODULE_LIST,     //!< Invalid module list, null pointer or bad magic
    /* End marker */
    _PLD_ERROR_MAX,
};

typedef enum pld_error pld_error_t;

/**
 * Get error name
 *
 * @param error - one of the enum variants
 * @return error name
 */
const char *pld_error_str(pld_error_t error);

#if PLD_HAVE_ALLOC
typedef void *(*pld_alloc_fn)(size_t capacity);
#endif /* PLD_HAVE_ALLOC */

/** Generic payload struct, for casting */
struct payload_generic {
    uint16_t pld_id; //!< Payload ID
};

/** Value indicating a payload ID was not given */
#define PAYLOAD_ID_NONE 0

/** Magic number that must be present in every pld_module */
#define PLD_MODULE_MAGIC 0xA5B6

struct pld_module {
    uint16_t magic;
    const char *name;
    const struct pld_spec * specs;
    uint16_t start_index;
    uint16_t count;
};

typedef struct pld_module pld_module_t;

/**
 * Helper macro to declare a list of supported modules.
 *
 * The macro is variadic and accepts any number of pointers to payload spec modules.
 */
#define PLD_MODULES(...) {__VA_ARGS__, NULL}

/**
 * CSP message source and destination descriptor.
 * Costruct using the csp2lpld() macro.
 *
 * Value 255 in src or dst means the field is not used for matching / is a wildcard.
 */
struct __attribute__((packed)) pld_address {
    uint8_t src;
    uint8_t sport;
    uint8_t dst;
    uint8_t dport;
};

/**
 * Convert `csp_id_t` to `struct pld_address`
 * (defined like this to avoid including libcsp)
 */
#define cspid2pldaddr(id) (struct pld_address) {(id).src, (id).sport, (id).dst, (id).dport}

/**
 * Get addressing for a payload
 *
 * The payload must have .pld_id
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] payload payload to inspect
 * @param[out] addr - addr is stored here (pointer to a struct)
 * @return success
 */
pld_error_t pld_get_addr(
    const pld_module_t **modules,
    const void *payload,
    struct pld_address *addr
);

/**
 * Get addressing for a payload ID
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] pld_id - payload ID
 * @param[out] addr - addr is stored here (pointer to a struct)
 * @return success
 */
pld_error_t pld_get_addr_by_id(
    const pld_module_t **modules,
    uint16_t pld_id,
    struct pld_address *addr
);

/**
 * Recognize a payload; may be used in place of pld_parse() if the payload data is not needed (yet).
 *
 * The payload can then be parsed using the obtained kind enum by pld_parse_as().
 *
 * This function is faster than pld_parse and does not allocate any memory.
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] addr - the message source and destination
 * @param[in] buffer - data buffer
 * @param[in] length - buffer length in bytes
 * @param[out] kind - the detected payload kind is output here
 * @return error status, LPLD_OK (0) on success
 */
pld_error_t pld_recognize(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    uint32_t *kind
);

#if PLD_HAVE_ALLOC
/**
 * Try to recognize and parse a payload. The payload is dynamically allocated.
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] addr - the message source and destination
 * @param[in] buffer - data buffer
 * @param[in] length - buffer length in bytes
 * @param[in] alloc - allocator
 * @param[out] output - the parsed payload is output here
 * @param[out] kind - the detected payload kind is output here (it is also stored as the first uint16_t field `.pld_id` of the payload)
 * @return error status, LPLD_OK (0) on success
 */
pld_error_t pld_parse(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    pld_alloc_fn alloc,
    void **output,
    uint32_t *kind
);

/**
 * Parse a payload previously recognized by pld_recognize() (or where the type is known at run-time).
 * Using the two functions manually avoids parsing payloads we are not interested in.
 *
 * If the type is known at build time, it's better to use the dedicated parse functions directly.
 *
 * The payload is dynamically allocated.
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] kind - payload kind to expect
 * @param[in] buffer - data buffer
 * @param[in] length - buffer length in bytes
 * @param[in] alloc - allocator
 * @param[out] output - the parsed payload is output here
 * @return error status, LPLD_OK (0) on success
 */
pld_error_t pld_parse_as(
    const pld_module_t **modules,
    uint32_t kind,
    const uint8_t *buffer,
    size_t length,
    pld_alloc_fn alloc,
    void **output
);

/**
 * Generic payload building function. The buffer is dynamically allocated.
 *
 * Packs any payload with a valid `pld_id` to its binary form.
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] payload - payload to pack
 * @param[in] alloc - allocator
 * @param[out] output - the binary buffer is output here
 * @param[out] length - length of the alocated buffer
 * @return success
 */
pld_error_t pld_build(
    const pld_module_t **modules,
    const void *payload,
    pld_alloc_fn alloc,
    uint8_t **output,
    size_t *length
);
#endif /* PLD_HAVE_ALLOC */

/**
 * Try to recognize and parse a payload with pre-allocated output buffer.
 *
 * @attention This function is UNSAFE, only call it when sure that the buffer is
 *            large enough for the largest known payload!
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] addr - the message source and destination
 * @param[in] buffer - data buffer
 * @param[in] length - buffer length in bytes
 * @param[out] output - byte buffer to cast to the payload struct
 * @param[out] kind - the detected payload kind is output here (it is also stored as the first uint16_t field `.pld_id` of the payload)
 * @return error status, LPLD_OK (0) on success
 */
pld_error_t pld_parse_s(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    void *output,
    uint32_t *kind
);

/**
 * Parse a payload previously recognized by pld_recognize() (or where the type is known at run-time).
 * Using the two functions manually avoids parsing payloads we are not interested in.
 *
 * If the type is known at build time, it's better to use the dedicated parse functions directly.
 *
 * @attention This function is UNSAFE, only call it when sure that the buffer is
 *            large enough for the payload struct!
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] kind - payload kind to expect
 * @param[in] buffer - data buffer
 * @param[in] length - buffer length in bytes
 * @param[out] output - byte buffer to cast to the payload struct
 * @return error status, LPLD_OK (0) on success
 */
pld_error_t pld_parse_as_s(
    const pld_module_t **modules,
    uint32_t kind,
    const uint8_t *buffer,
    size_t length,
    void *output
);

/**
 * Generic payload building function with static pre-allocation.
 * Packs any payload with a valid `pld_id` to its binary form,
 * provided it fits in the buffer.
 *
 * This function is safe, buffer length is checked and an error
 * is returned when it is not large enough.
 *
 * @param[in] modules - list of payload modules to search
 * @param[in] payload - payload to pack
 * @param[in,out] output - the binary buffer to modify
 * @param[in] capacity - length of the pre-alocated buffer
 * @return success
 */
pld_error_t pld_build_s(
    const pld_module_t **modules,
    const void *payload,
    uint8_t *output,
    size_t capacity
);

#if PLD_HAVE_EXPORT

#if !PLD_HAVE_ALLOC
#error "To use export functions, alloc must be enabled."
#endif

/**
 * JSON formatter style variants
 */
enum pld_export_json_style {
    /** Plain data-only JSON, values are directly in root; no "name" field added */
    EXPORT_JSON_DATAONLY,

    /** {"name":"Name","descr":"Descr","data":{...}} */
    EXPORT_JSON_BASIC,

    /** Similar to BASIC, but data fields are each wrapped in a descriptive object. */
    EXPORT_JSON_ANNOTATED,

    _EXPORT_JSON_MAX,
};

/**
 * Convert a parsed payload to JSON.
 *
 * Use `cJSON_Delete(json)` to deallocate it.
 *
 * @param[in] addr - payload address
 * @param[in] data - the parsed struct (using `void *` to allow any payload type)
 * @param[in] style - output style
 * @param[out] output - output pointer, will be populated with a cJSON Object on success.
 * @return success
 */
pld_error_t pld_to_json(
    const pld_module_t **modules,
    const void *pld_struct,
    enum pld_export_json_style style,
    cJSON** output
);

/**
 * Export a payload in JSON form to a nice human-readable format.
 * Supports and auto-detects all 3 formats (DATAONLY, BASIC, ANNOTATED)
 * produced by libpayload.
 *
 * @param[in] pld - JSON object
 * @param[in] colors - use terminal colors
 * @return the payload rendered to a dynamically allocated string
 */
char *pld_pretty_print(cJSON *pld, bool colors);

#endif /* PLD_HAVE_EXPORT */

#endif /* PAYLOADS_H */
