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

ERR_F lbmct_t_init(lbmct_t *ct)
{
  ct->ctx = NULL;

  return ERR_OK;
}  /* lbmct_t_init */


ERR_F lbmct_new(lbmct_t **rtn_ct)
{
  lbmct_t *ct;
  PRT_MALLOC_N(ct, lbmct_t, 1);

  ERR(lbmct_t_init(ct));

  *rtn_ct = ct;
  return ERR_OK;
}  /* lbmct_new */


ERR_F lbmct_create_e(lbmct_t **rtn_ct, lbm_context_t *ctx)
{
  lbmct_t *ct;

  ERR(lbmct_new(&ct));

  ct->ctx = ctx;

  *rtn_ct = ct;
  return ERR_OK;
}  /* lbmct_create_e */


int lbmct_create(lbmct_t **rtn_ct, lbm_context_t *ctx)
{
  err_t *err;

  err = lbmct_create_e(rtn_ct, ctx);
  if (err) {
    lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",
      BASENAME(__FILE__), __LINE__, err->mesg);
    err_dispose(err);
    return 1;
  }

  return LBM_OK;
}  /* lbmct_create */
