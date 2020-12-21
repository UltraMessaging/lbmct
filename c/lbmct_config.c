/*
 * See https://github.com/UltraMessaging/lbmct for code and documentation.
 *
 * Copyright (c) 2018-2019 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 -
 - This receiver code example is provided by Informatica for educational
 - and evaluation purposes only.
 -
 - THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 - EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 - NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 - INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 - OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
 - LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 - INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 - TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED
 - OF THE LIKELIHOOD OF SUCH DAMAGES.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include <lbm/lbm.h>
#include "lbmct.h"

ERR_F lbmct_config_t_init(lbmct_config_t *config)
{
  config->ctx = NULL;

  return ERR_OK;
}  /* lbmct_config_t_init */


ERR_F lbmct_config_new(lbmct_config_t **rtn_config)
{
  lbmct_config_t *config;
  PRT_MALLOC_N(config, lbmct_config_t, 1);

  ERR(lbmct_config_t_init(config));

  *rtn_config = config;
  return ERR_OK;
}  /* lbmct_config_new */


ERR_F lbmct_config_create_e(lbmct_config_t **rtn_config, lbm_context_t *ctx)
{
  lbmct_config_t *config;

  ERR(lbmct_config_new(&config));

  config->ctx = ctx;

  *rtn_config = config;
  return ERR_OK;
}  /* lbmct_config_create_e */


int lbmct_config_create(lbmct_config_t **rtn_config, lbm_context_t *ctx)
{
  err_t *err;

  err = lbmct_config_create_e(rtn_config, ctx);
  if (err) {
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",
      BASENAME(__FILE__), __LINE__, err->mesg);
    err_dispose(err);
    return 1;
  }

  return LBM_OK;
}  /* lbmct_config_create */
