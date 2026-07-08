/**
 * Payloads module configuration
 * This is a template for CMake to generate a config file
 */

#ifndef LPLDGEN_CONF_PAYLOADS_H
#define LPLDGEN_CONF_PAYLOADS_H

// Include the JSON and pretty-print export functions.
#define PLD_HAVE_EXPORT 0

// Include payload functions that use dynamic allocation.
// This option is required for the non-`_s` functions. (i.e. `pld_parse()`)
//
// Default = ON
#define PLD_HAVE_ALLOC 0

// Include the internal logging system, useful for debugging.
// The `pld_log_write()` function must be provided externally to use logging.
//
// Default = OFF
#define PLD_HAVE_LOGGING 0

// Have build functions verify payload ID.
// Values: 0 ... disabled, 1 ... always, 2 ... if given (do not validate if id = 0)
//
// Default = 2
#define PLD_BUILD_FUNCS_CHECK_ID 2

#endif //LPLDGEN_CONF_PAYLOADS_H
