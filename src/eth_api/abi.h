#include "../core/util/bytes.h"

typedef enum {
  A_UINT    = 1,
  A_INT     = 2,
  A_BYTES   = 3,
  A_BOOL    = 4,
  A_ADDRESS = 5,
  A_TUPLE   = 6,
  A_STRING  = 7
} atype_t;

typedef struct el {
  atype_t type;
  bytes_t data;
  uint8_t type_len;
  int     array_len;
} var_t;

typedef struct {
  var_t*           in_data;
  var_t*           out_data;
  bytes_builder_t* call_data;
  var_t*           current;
  char*            error;
  int              data_offset;
} call_request_t;

call_request_t* parseSignature(char* sig);
