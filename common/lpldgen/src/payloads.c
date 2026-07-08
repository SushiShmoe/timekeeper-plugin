#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#include "payloads.h"
#include "payloads_internal.h"

static const char * LPLD_ERROR_NAMES[] = {
    [PLD_OK] = "OK",
    [PLD_ERR] = "Error",
    [PLD_ERR_ALLOC] = "Allocation failed",
    [PLD_ERR_UNKNOWN_PAYLOAD] = "Unknown payload",
    [PLD_ERR_LENGTH] = "Bad buffer length",
    [PLD_ERR_TAIL_LENGTH] = "Bad tail length",
    [PLD_ERR_BAD_ARGS] = "Bad args",
    [PLD_ERR_NOT_IMPLEMENTED] = "Not implemented",
    [PLD_ERR_ID_MISMATCH] = "Payload ID mismatch",
    [PLD_ERR_MODULE_LIST] = "Invalid module list",

    // safeguard against read out of range if a translation is missing
    [_PLD_ERROR_MAX] = NULL,
};

const char *pld_error_str(pld_error_t error) {
    if (error < _PLD_ERROR_MAX) {
        if (LPLD_ERROR_NAMES[error]) { // not NULL
            return LPLD_ERROR_NAMES[error];
        }
    }

    return "Unknown Error";
}


/**
 * Test for address match
 *
 * @param[in] addr - tested payload's address
 * @param[in] template - address from the template table
 * @return match
 */
static inline bool address_matches(const struct pld_address *addr, const struct pld_address *template) {
    if (template->src != 255) {
        return template->src == addr->src && template->sport == addr->sport;
    }
    else if (template->dst != 255) {
        return template->dst == addr->dst && template->dport == addr->dport;
    }

    return false; /* this should be unreachable */
}

pld_error_t pld_find_spec_by_kind_in_module(const pld_module_t *module, uint32_t kind, const struct pld_spec ** result) {
    pld_log_trace("pld_find_spec_by_kind_in_module %"PRIu32, kind);

    if (kind == PAYLOAD_ID_NONE) {
        pld_log_error("kind=0");
        return PLD_ERR_BAD_ARGS;
    }

    if (module->magic != PLD_MODULE_MAGIC) {
        pld_log_error("module bad magic!");
        return PLD_ERR_MODULE_LIST;
    }

    if (kind >= module->start_index && kind < module->start_index + module->count) {
        *result = &module->specs[kind - module->start_index];
        pld_log_debug("spec found in %s", module->name);
        return PLD_OK;
    }
    pld_log_debug("spec not found in module %s", module->name);
    return PLD_ERR_UNKNOWN_PAYLOAD;
}

pld_error_t pld_find_spec_by_kind(const pld_module_t **modules, uint32_t kind, const struct pld_spec ** result) {
    pld_log_trace("pld_find_spec_by_kind %"PRIu32, kind);

    if (kind == PAYLOAD_ID_NONE) {
        pld_log_error("kind=0");
        return PLD_ERR_BAD_ARGS;
    }

    while (*modules != NULL) {
        const pld_module_t *entry = *modules;
        pld_error_t rv = pld_find_spec_by_kind_in_module(entry, kind, result);
        switch (rv) {
            case PLD_OK:
                return PLD_OK;
            case PLD_ERR_UNKNOWN_PAYLOAD:
                goto next;
            default:
                return rv; // error
        }
    next:
        modules++;
    }
    pld_log_warn("payload spec not found in any module");
    return PLD_ERR_UNKNOWN_PAYLOAD;
}

/**
 * Variant of `pld_recognize()` that also returns the found `pld_spec`
 * (to avoid repeating the look-up in the parse function)
 *
 * @param modules
 * @param addr
 * @param buffer
 * @param length
 * @param kind
 * @param spec
 * @return
 */
static pld_error_t pld_recognize_internal(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    uint32_t *output_kind,
    const struct pld_spec **output_spec
) {
    pld_log_trace("pld_recognize_internal");

    while (*modules != NULL) {
        const pld_module_t *module = *modules;
        if (module->magic != PLD_MODULE_MAGIC) {
            pld_log_error("module bad magic!");
            return PLD_ERR_MODULE_LIST;
        }

        for (size_t i = 0; i < module->count; i++) {
            const struct pld_spec *spec = &module->specs[i];
            uint16_t expected = module->start_index + i;
            if (spec->pld_id != expected) {
                pld_log_error("spec %s id=%"PRIu16", expected %"PRIu16". module %s error!", spec->name, spec->pld_id, expected, module->name);
                return PLD_ERR_MODULE_LIST;
            }

            if (spec->recognize && address_matches(&addr, &spec->address)) {
                pld_log_trace("address match for %s", spec->name);
                if (spec->bin_size == length || (spec->growable && spec->bin_size < length)) {
                    pld_log_trace("length match for %s", spec->name);
                    if (NULL == spec->tester || spec->tester(buffer)) {
                        pld_log_trace("content match for %s", spec->name);
                        if (output_kind != NULL) {
                            *output_kind = module->start_index + i;
                        }
                        if (output_spec != NULL) {
                            *output_spec = spec;
                        }
                        pld_log_debug("recognized as %s from %s", spec->name, module->name);
                        return PLD_OK;
                    }
                }
            }
        }
        modules++;
    }
    pld_log_warn("payload not recognized");
    return PLD_ERR_UNKNOWN_PAYLOAD;
}

#if PLD_HAVE_ALLOC
pld_error_t pld_parse(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    pld_alloc_fn alloc,
    void **output,
    uint32_t *kind
) {
    pld_log_trace("pld_parse");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_recognize_internal(modules, addr, buffer, length, kind, &spec);
    if (rv != PLD_OK) {
        return rv;
    }
    return spec->parse(buffer, length, alloc, output);
}

pld_error_t pld_parse_as(
    const pld_module_t **modules,
    uint32_t kind,
    const uint8_t *buffer,
    size_t length,
    pld_alloc_fn alloc,
    void **output
) {
    pld_log_trace("pld_parse_as");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_find_spec_by_kind(modules, kind, &spec);
    if (rv != PLD_OK) return rv;
    return spec->parse(buffer, length, alloc, output);
}
#endif /* PLD_HAVE_ALLOC */

pld_error_t pld_parse_s(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    void *output,
    uint32_t *kind
) {
    pld_log_trace("pld_parse_s");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_recognize_internal(modules, addr, buffer, length, kind, &spec);
    if (rv != PLD_OK) {
        return rv;
    }
    return spec->parse_static(buffer, length, output);
}

pld_error_t pld_parse_as_s(
    const pld_module_t **modules,
    uint32_t kind,
    const uint8_t *buffer,
    size_t length,
    void *output
) {
    pld_log_trace("pld_parse_as_s");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_find_spec_by_kind(modules, kind, &spec);
    if (rv != PLD_OK) return rv;
    return spec->parse_static(buffer, length, output);
}

pld_error_t pld_recognize(
    const pld_module_t **modules,
    struct pld_address addr,
    const uint8_t *buffer,
    size_t length,
    uint32_t *kind
) {
    pld_log_trace("pld_recognize");

    return pld_recognize_internal(modules, addr, buffer, length, kind, NULL);
}

static pld_error_t pld_build_common(
    const pld_module_t **modules,
    const void *payload,
    const struct pld_spec **spec
) {
    pld_log_trace("pld_build_common");

    if (!modules) {
        pld_log_error("modules is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!payload) {
        pld_log_error("payload is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    struct payload_generic *cast = (struct payload_generic *) payload;
    uint16_t kind = cast->pld_id;
    if (kind == PAYLOAD_ID_NONE) {
        pld_log_error("payload.pld_id=0, can't lookup");
        return PLD_ERR_ID_MISMATCH; // zero ID is illegal
    }

    return pld_find_spec_by_kind(modules, kind, spec);
}

#if PLD_HAVE_ALLOC
pld_error_t pld_build(
    const pld_module_t **modules,
    const void *payload,
    pld_alloc_fn alloc,
    uint8_t **output,
    size_t *length
) {
    pld_log_trace("pld_build");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_build_common(modules, payload, &spec);
    if (rv != PLD_OK) return rv;
    return spec->build(payload, alloc, output, length);
}
#endif /* PLD_HAVE_ALLOC */


pld_error_t pld_build_s(
    const pld_module_t **modules,
    const void *payload,
    uint8_t *output,
    size_t capacity
) {
    pld_log_trace("pld_build_s");

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_build_common(modules, payload, &spec);
    if (rv != PLD_OK) return rv;
    return spec->build_static(payload, output, capacity);
}

pld_error_t pld_get_addr_by_id(
    const pld_module_t **modules,
    uint16_t pld_id,
    struct pld_address *addr
) {
    pld_log_trace("pld_get_addr_by_id");

    if (!modules) {
        pld_log_error("modules is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!addr) {
        pld_log_error("addr is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (pld_id == PAYLOAD_ID_NONE) {
        pld_log_error("pld_id can't be 0");
        return PLD_ERR_BAD_ARGS;
    }

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_find_spec_by_kind(modules, pld_id, &spec);
    if (rv != PLD_OK) return rv;
    *addr = spec->address;
    return PLD_OK;
}

pld_error_t pld_get_addr(
    const pld_module_t **modules,
    const void *payload,
    struct pld_address *addr
) {
    pld_log_trace("pld_get_addr");

    if (!modules) {
        pld_log_error("modules is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!addr) {
        pld_log_error("addr is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!payload) {
        pld_log_error("payload is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    struct payload_generic *cast = (struct payload_generic *) payload;
    uint16_t kind = cast->pld_id;
    if (kind == PAYLOAD_ID_NONE) {
        pld_log_error("payload.pld_id=0, can't lookup");
        return PLD_ERR_ID_MISMATCH; // zero ID is illegal
    }

    return pld_get_addr_by_id(modules, kind, addr);
}


#if PLD_HAVE_EXPORT
#include <string.h>

pld_error_t pld_to_json(
    const pld_module_t **modules,
    const void *pld_struct,
    enum pld_export_json_style style,
    cJSON** output
) {
    pld_log_trace("pld_to_json");

    if (!modules) {
        pld_log_error("modules is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!pld_struct) {
        pld_log_error("pld_struct is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    struct payload_generic *cast = (struct payload_generic *) pld_struct;
    uint16_t kind = cast->pld_id;
    if (kind == PAYLOAD_ID_NONE) {
        pld_log_error("payload.pld_id=0, can't lookup");
        return PLD_ERR_ID_MISMATCH; // zero ID is illegal
    }

    const struct pld_spec *spec = NULL;
    pld_error_t rv = pld_find_spec_by_kind(modules, kind, &spec);
    if (rv != PLD_OK) {
        return rv;
    }

    return pld_to_json_by_spec(spec, pld_struct, style, output);
}

pld_error_t pld_to_json_by_spec(
    const struct pld_spec *spec,
    const void *pld_struct,
    enum pld_export_json_style style,
    cJSON** output
) {
    pld_log_trace("pld_to_json_by_spec");

    if (!spec) {
        pld_log_error("spec is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (!pld_struct) {
        pld_log_error("pld_struct is NULL");
        return PLD_ERR_BAD_ARGS;
    }

    if (style >= _EXPORT_JSON_MAX) {
        return PLD_ERR_NOT_IMPLEMENTED;
    }

    if (spec->data_to_json) {
        cJSON *object = NULL;
        CJSON_TRY(object = cJSON_CreateObject());
        *output = object;

        if (style != EXPORT_JSON_DATAONLY) {
            // Add common fields
            CJSON_TRY(cJSON_AddStringToObject(object, "name", spec->name));

            if (style == EXPORT_JSON_ANNOTATED && spec->descr) {
                CJSON_TRY(cJSON_AddStringToObject(object, "descr", spec->descr));
            }

            cJSON *dataObject = NULL;
            CJSON_TRY(dataObject = cJSON_CreateObject());
            cJSON_AddItemToObject(object, "data", dataObject);

            // the internal function does not understand EXPORT_JSON_BASIC
            if (style == EXPORT_JSON_BASIC) {
                style = EXPORT_JSON_DATAONLY;
            }

            // Add payload content
            pld_error_t rv = spec->data_to_json(pld_struct, style, dataObject);
            if (rv != PLD_OK) {
                pld_log_error("data_to_json failed");
                cJSON_Delete(*output);
                *output = NULL;
            }
            return rv;
        } else {
            // dataonly
            pld_error_t rv = spec->data_to_json(pld_struct, style, object);
            if (rv != PLD_OK) {
                pld_log_error("data_to_json failed");
                cJSON_Delete(*output);
                *output = NULL;
            }
            return rv;
        }

    cjfail:
        pld_log_error("JSON alloc failed");
        cJSON_Delete(*output);
        *output = NULL;
        return PLD_ERR_ALLOC;
    } else {
        pld_log_error("no data_to_json func for spec");
        return PLD_ERR_NOT_IMPLEMENTED;
    }
}

#endif /* PLD_HAVE_EXPORT */
