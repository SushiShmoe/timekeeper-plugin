#ifndef CMD_CLI_H
#define CMD_CLI_H

#include <service/service_com.h>

#if defined(SHARED_BUILD)
/*!
  \brief Register client commands.
  This signature is mandatory for VCOM plugins!
*/
void register_cli_cmds(void);

/*!
  \brief Function to provide caller with version control information about the plugin.
  This signature is mandatory for VCOM plugins!
  \param semver Pointer to buffer for storing semantic version string
  (15 characters at most).
  \param gitrev Pointer to buffer for storing abbreviated git revision string
  (13 characters at most when "-dirty").
*/
void get_version(char* semver, char* gitrev);
#endif

/*!
  \brief Register client commands.
  This function can be used when multiple statically linked client libraries are used.
  Note that the name has to be unique for the client!
*/
void register_tk_cmds(void);

#endif