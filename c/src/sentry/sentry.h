#define __USE_XOPEN_EXTENDED 1
#define __USE_XOPEN          1
#include "../third-party/sentry-native/include/sentry.h"
#include "../core/client/plugin.h"
#include "../core/client/client.h"
#include "../core/client/version.h"
#include "../core/util/log.h"
#include "../core/util/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * the security- configuration which is stored within the chain_t - object.
 */
typedef struct sentry_conf {
  const char* dsn;
  const char* db;
  uint8_t     debug;
  uint8_t     stack;
} sentry_conf_t;

in3_ret_t in3_register_sentry(in3_t* c);