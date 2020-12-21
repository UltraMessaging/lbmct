/* main.cc
 *
 * See https://github.com/UltraMessaging/lbmct
 *
 * Copyright (c) 2005-2018 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 *
 * This source code example is provided by Informatica for educational
 * and evaluation purposes only.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 * OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE LIABLE TO  * LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR INDIRECT
 * DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE TRANSACTIONS
 * CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF THE
 * LIKELIHOOD OF SUCH DAMAGES.
 */

#include "gtest/gtest.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <lbm/lbm.h>
#include "lbmct.h"
#include "prt.h"

/* Global (used for clientd testing). */
char *null_ptr = NULL;
lbm_context_t *ctx1 = NULL;
lbm_context_t *ctx2 = NULL;

#define LBM_ERR(_e) do {\
  if ((_e) != LBM_OK) {\
    fprintf(stderr, "LBM_ERR failed at %s:%d (%s)\n", BASENAME(__FILE__), __LINE__, lbm_errmsg());\
    fflush(stderr);\
    abort();\
  }\
} while (0)


TEST(Ct,CtCreateDeleteTest) {
  lbmct_t *ct = NULL;
  int err;

#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
  SLEEP_MSEC(100);

  err = lbmct_create(&ct, ctx1);
  ASSERT_EQ(0, err) << lbm_errmsg();
}  /* Ct,CtCreateDeleteTest */


int main(int argc, char **argv) {
  lbm_context_attr_t *ctx_attr;
  const char *cfg1, *cfg2;
  int err;

  ::testing::InitGoogleTest(&argc, argv);

  if (argc == 1) {
    cfg1 = "test.cfg";
    cfg2 = cfg1;
  }
  else if (argc == 2) {
    cfg1 = argv[1];
    cfg2 = cfg1;
  }
  else if (argc == 3) {
    cfg1 = argv[1];
    cfg2 = argv[2];
  }
  else {
    printf("unrecognized command line\n");
    exit(1);
  }

#if defined(_MSC_VER)
  /* windows-specific code */
  WSADATA wsadata;
  int wsStat = WSAStartup(MAKEWORD(2,2), &wsadata);
  if (wsStat != 0) {
    printf("line %d: wsStat=%d\n",__LINE__,wsStat); exit(1);
  }
#endif

  /* Not sure why I need to do this with MacOS.  Not sure why some of the
   * tests generates a sigpipe on Mac but not Linux.
   */
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  err = lbm_config(cfg1);
  LBM_ERR(err);

  /* Create a pair of contexts, one for publisher, the other for subscriber. */

  /* On context request ports, disable Nagle's alg (turn on NODELAY).  Also
   * turn on MIM to verify that a MIM messsge doesn't cause a problem.
   */
  err = lbm_context_attr_create(&ctx_attr);
  LBM_ERR(err);
  err = lbm_context_attr_str_setopt(ctx_attr,
    "response_tcp_nodelay", "1");
  LBM_ERR(err);

  err = lbm_context_create(&ctx1, ctx_attr, NULL, NULL);
  LBM_ERR(err);

  err = lbm_context_attr_delete(ctx_attr);
  LBM_ERR(err);

  /* If different configs, assume DRO. */
  if (cfg1 != cfg2) {
    SLEEP_MSEC(1500);  /* Let ctx1 discover its domain ID. */

    err = lbm_config(cfg2);
    LBM_ERR(err);
  }

  err = lbm_context_attr_create(&ctx_attr);
  LBM_ERR(err);
  err = lbm_context_attr_str_setopt(ctx_attr,
    "response_tcp_nodelay", "1");
  LBM_ERR(err);

  err = lbm_context_create(&ctx2, ctx_attr, NULL, NULL);
  LBM_ERR(err);

  /* If different configs, assume DRO. */
  if (cfg1 != cfg2) {
    SLEEP_MSEC(1500);  /* Let ctx2 discover its domain ID. */
  }

  err = lbm_context_attr_delete(ctx_attr);
  LBM_ERR(err);

  err = RUN_ALL_TESTS();

  (void)lbm_context_delete(ctx1);
  (void)lbm_context_delete(ctx2);

  return 0;
}  /* main */
