/*******************************************************************************
 * This file is part of the Incubed project.
 * Sources: https://github.com/blockchainsllc/in3
 *
 * Copyright (C) 2018-2020 slock.it GmbH, Blockchains LLC
 *
 *
 * COMMERCIAL LICENSE USAGE
 *
 * Licensees holding a valid commercial license may use this file in accordance
 * with the commercial license agreement provided with the Software or, alternatively,
 * in accordance with the terms contained in a written agreement between you and
 * slock.it GmbH/Blockchains LLC. For licensing terms and conditions or further
 * information please contact slock.it at in3@slock.it.
 *
 * Alternatively, this file may be used under the AGPL license as follows:
 *
 * AGPL LICENSE USAGE
 *
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 * [Permissions of this strong copyleft license are conditioned on making available
 * complete source code of licensed works and modifications, which include larger
 * works using a licensed work, under the same license. Copyright and license notices
 * must be preserved. Contributors provide an express grant of patent rights.]
 * You should have received a copy of the GNU Affero General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 *******************************************************************************/

#include "request.h"
#include "../util/debug.h"
#include "../util/log.h"
#include "client.h"
#include "keys.h"
#include "plugin.h"
#include "request_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static in3_ret_t in3_plugin_init(in3_req_t* ctx) {
  if ((ctx->client->plugin_acts & PLGN_ACT_INIT) == 0) return IN3_OK;
  for (in3_plugin_t* p = ctx->client->plugins; p; p = p->next) {
    if (p->acts & PLGN_ACT_INIT) {
      TRY(p->action_fn(p->data, PLGN_ACT_INIT, ctx))
      p->acts &= ~((uint64_t) PLGN_ACT_INIT);
    }
  }
  ctx->client->plugin_acts &= ~((uint64_t) PLGN_ACT_INIT);
  return IN3_OK;
}

bool in_property_name(char* c) {
  for (c++; *c && *c != '"'; c++) {
    if (*c == '\\') c++;
  }
  if (*c) c++;
  while (*c && (*c == ' ' || *c < 14)) c++;
  return *c == ':';
}

in3_req_t* req_new_clone(in3_t* client, const char* req_data) {
  char*      data = _strdupn(req_data, -1);
  in3_req_t* r    = req_new(client, data);
  if (r)
    in3_cache_add_ptr(&r->cache, data);
  else
    _free(data);
  return r;
}

void in3_set_chain_id(in3_req_t* req, chain_id_t id) {
  if (!id || in3_chain_id(req) == id) return;

  cache_entry_t* entry = in3_cache_add_entry(&req->cache, NULL_BYTES, NULL_BYTES);
  entry->props         = CACHE_PROP_CHAIN_ID | CACHE_PROP_INHERIT;
  int_to_bytes((uint32_t) id, entry->buffer);
}

chain_id_t in3_chain_id(const in3_req_t* req) {
  for (cache_entry_t* entry = req->cache; entry; entry = entry->next) {
    if (entry->props & CACHE_PROP_CHAIN_ID)
      return (chain_id_t) bytes_to_int(entry->buffer, 4);
  }
  return req->client->chain.id;
}

in3_chain_t* in3_get_chain(in3_t* c, chain_id_t id) {
  in3_chain_t* chain = &c->chain;
  while (chain) {
    if (chain->id == id) return chain;
    if (chain->next)
      chain = chain->next;
    else
      break;
  }
  chain->next = _calloc(1, sizeof(in3_chain_t));
  chain       = chain->next;
  chain->id   = id;
  switch (id) {
    case CHAIN_ID_BTC:
      chain->type = CHAIN_BTC;
      break;
    case CHAIN_ID_IPFS:
      chain->type = CHAIN_IPFS;
      break;
    default:
      chain->type = CHAIN_ETH;
  }
  return chain;
}

in3_req_t* req_new(in3_t* client, const char* req_data) {
  assert_in3(client);
  assert(req_data);

  if (client->pending == 0xFFFF) return NULL; // avoid overflows by not creating any new ctx anymore
  in3_req_t* ctx = _calloc(1, sizeof(in3_req_t));
  if (!ctx) return NULL;
  ctx->client             = client;
  ctx->verification_state = IN3_WAITING;
  client->pending++;

  if (req_data != NULL) {
    ctx->request_context = parse_json(req_data);
    if (!ctx->request_context) {
      in3_log_error("Invalid json-request: %s\n", req_data);
      req_set_error(ctx, "Error parsing the JSON-request!", IN3_EINVAL);
      char* msg = parse_json_error(req_data);
      if (msg) {
        req_set_error(ctx, msg, IN3_EINVAL);
        _free(msg);
      }
      return ctx;
    }

    if (d_type(ctx->request_context->result) == T_OBJECT) {
      // it is a single result
      ctx->requests    = _malloc(sizeof(d_token_t*));
      ctx->requests[0] = ctx->request_context->result;
      ctx->len         = 1;
    }
    else if (d_type(ctx->request_context->result) == T_ARRAY) {
      // we have an array, so we need to store the request-data as array
      d_token_t* t  = d_get_at(ctx->request_context->result, 0);
      ctx->len      = d_len(ctx->request_context->result);
      ctx->requests = _malloc(sizeof(d_token_t*) * ctx->len);
      for (uint_fast16_t i = 0; i < ctx->len; i++, t = d_next(t))
        ctx->requests[i] = t;
    }
    else {
      req_set_error(ctx, "The Request is not a valid structure!", IN3_EINVAL);
      return ctx;
    }

    d_token_t* t = d_get(ctx->request_context->result, K_ID);
    if (t == NULL) {
      ctx->id = client->id_count;
      client->id_count += ctx->len;
    }
    else if (d_type(t) == T_INTEGER)
      ctx->id = d_int(t);

    in3_set_chain_id(ctx, (chain_id_t) d_get_long(d_get(ctx->requests[0], K_IN3), K_CHAIN_ID));
  }
  // if this is the first request, we initialize the plugins now
  in3_plugin_init(ctx);
  return ctx;
}

char* req_get_error_data(in3_req_t* ctx) {
  return ctx ? ctx->error : "No request context";
}

char* req_get_result_json(in3_req_t* ctx, int index) {
  assert_in3_req(ctx);
  if (!ctx->responses) return NULL;
  d_token_t* res = d_get(ctx->responses[index], K_RESULT);
  return res ? d_create_json(ctx->response_context, res) : NULL;
}

char* req_get_response_data(in3_req_t* ctx) {
  assert_in3_req(ctx);

  sb_t sb = {0};
  if (d_type(ctx->request_context->result) == T_ARRAY) sb_add_char(&sb, '[');
  for (uint_fast16_t i = 0; i < ctx->len; i++) {
    if (i) sb_add_char(&sb, ',');
    str_range_t rr    = d_to_json(ctx->responses[i]);
    char*       start = NULL;
    if ((ctx->client->flags & FLAGS_KEEP_IN3) == 0 && (start = d_to_json(d_get(ctx->responses[i], K_IN3)).data) && start < rr.data + rr.len) {
      while (*start != ',' && start > rr.data) start--;
      sb_add_range(&sb, rr.data, 0, start - rr.data + 1);
      sb.data[sb.len - 1] = '}';
    }
    else
      sb_add_range(&sb, rr.data, 0, rr.len);
  }
  if (d_type(ctx->request_context->result) == T_ARRAY) sb_add_char(&sb, ']');
  return sb.data;
}

req_type_t req_get_type(in3_req_t* ctx) {
  assert_in3_req(ctx);
  return ctx->type;
}

in3_ret_t req_check_response_error(in3_req_t* c, int i) {
  assert_in3_req(c);

  d_token_t* r = d_get(c->responses[i], K_ERROR);
  if (!r)
    return IN3_OK;
  else if (d_type(r) == T_OBJECT) {
    str_range_t s   = d_to_json(r);
    char*       req = alloca(s.len + 1);
    strncpy(req, s.data, s.len);
    req[s.len] = '\0';
    return req_set_error(c, req, IN3_ERPC);
  }
  else
    return req_set_error(c, d_string(r), IN3_ERPC);
}

in3_ret_t req_set_error_intern(in3_req_t* ctx, char* message, in3_ret_t errnumber, const char* filename, const char* function, int line) {
  assert(ctx);

  // if this is just waiting, it is not an error!
  if (errnumber == IN3_WAITING || errnumber == IN3_OK) return errnumber;
  if (message) {

    const size_t l   = strlen(message);
    char*        dst = NULL;
    if (ctx->error) {
      dst = _malloc(l + 2 + strlen(ctx->error));
      strcpy(dst, message);
      dst[l] = ':';
      strcpy(dst + l + 1, ctx->error);
      _free(ctx->error);
    }
    else {
      dst = _malloc(l + 1);
      strcpy(dst, message);
    }
    ctx->error = dst;

    error_log_ctx_t sctx = {.msg = message, .error = -errnumber, .req = ctx};
    in3_plugin_execute_first_or_none(ctx, PLGN_ACT_LOG_ERROR, &sctx);

    if (filename && function)
      in3_log_trace("Intermediate error -> %s in %s:%i %s()\n", message, filename, line, function);
    else
      in3_log_trace("Intermediate error -> %s\n", message);
  }
  else if (!ctx->error) {
    ctx->error    = _malloc(2);
    ctx->error[0] = 'E';
    ctx->error[1] = 0;
  }
  ctx->verification_state = errnumber;
  return errnumber;
}

in3_ret_t req_get_error(in3_req_t* ctx, int id) {
  if (ctx->error)
    return IN3_ERPC;
  else if (id >= (int) ctx->len)
    return IN3_EINVAL;
  else if (!ctx->responses || !ctx->responses[id])
    return IN3_ERPCNRES;
  else if (NULL == d_get(ctx->responses[id], K_RESULT) || d_get(ctx->responses[id], K_ERROR))
    return IN3_EINVALDT;
  return IN3_OK;
}

void in3_req_free_nodes(node_match_t* node) {
  node_match_t* last_node = NULL;
  while (node) {
    last_node = node;
    node      = node->next;
    _free(last_node->url);
    _free(last_node);
  }
}

int req_nodes_len(node_match_t* node) {
  int all = 0;
  while (node) {
    all++;
    node = node->next;
  }
  return all;
}

bool req_is_method(const in3_req_t* ctx, const char* method) {
  const char* required_method = d_get_string(ctx->requests[0], K_METHOD);
  return (required_method && strcmp(required_method, method) == 0);
}

in3_proof_t in3_req_get_proof(in3_req_t* ctx, int i) {
  if (ctx->requests) {
    char* verfification = d_get_string(d_get(ctx->requests[i], K_IN3), key("verification"));
    if (verfification && strcmp(verfification, "none") == 0) return PROOF_NONE;
    if (verfification && strcmp(verfification, "proof") == 0) return PROOF_STANDARD;
  }
  if (ctx->signers_length && !ctx->client->proof) return PROOF_STANDARD;
  return ctx->client->proof;
}

NONULL void in3_req_add_response(
    in3_http_request_t* req,      /**< [in]the the request */
    int                 index,    /**< [in] the index of the url, since this request could go out to many urls */
    int                 error,    /**< [in] if true this will be reported as error. the message should then be the error-message */
    const char*         data,     /**<  the data or the the string*/
    int                 data_len, /**<  the length of the data or the the string (use -1 if data is a null terminated string)*/
    uint32_t            time) {
  in3_ctx_add_response(req->req, index, error, data, data_len, time);
}

void in3_ctx_add_response(
    in3_req_t*  ctx,      /**< [in] the context */
    int         index,    /**< [in] the index of the url, since this request could go out to many urls */
    int         error,    /**< [in] if true this will be reported as error. the message should then be the error-message */
    const char* data,     /**<  the data or the the string*/
    int         data_len, /**<  the length of the data or the the string (use -1 if data is a null terminated string)*/
    uint32_t    time) {

  assert_in3_req(ctx);
  assert(data);
  if (error == 1) error = IN3_ERPC;

  if (!ctx->raw_response) {
    req_set_error(ctx, "no request created yet!", IN3_EINVAL);
    return;
  }
  in3_response_t* response = ctx->raw_response + index;
  response->time += time;
  if (response->state == IN3_OK && error) response->data.len = 0;
  response->state = error;
  if (data_len == -1)
    sb_add_chars(&response->data, data);
  else
    sb_add_range(&response->data, data, 0, data_len);
}

sb_t* in3_rpc_handle_start(in3_rpc_handle_ctx_t* hctx) {
  assert(hctx);
  assert_in3_req(hctx->req);
  assert(hctx->request);
  assert(hctx->response);

  *hctx->response = _calloc(1, sizeof(in3_response_t));
  sb_add_chars(&(*hctx->response)->data, "{\"id\":");
  sb_add_int(&(*hctx->response)->data, hctx->req->id);
  return sb_add_chars(&(*hctx->response)->data, ",\"jsonrpc\":\"2.0\",\"result\":");
}
in3_ret_t in3_rpc_handle_finish(in3_rpc_handle_ctx_t* hctx) {
  sb_add_char(&(*hctx->response)->data, '}');
  return IN3_OK;
}

in3_ret_t in3_rpc_handle_with_bytes(in3_rpc_handle_ctx_t* hctx, bytes_t data) {
  sb_add_bytes(in3_rpc_handle_start(hctx), NULL, &data, 1, false);
  return in3_rpc_handle_finish(hctx);
}

in3_ret_t in3_rpc_handle_with_uint256(in3_rpc_handle_ctx_t* hctx, bytes_t data) {
  b_optimize_len(&data);
  sb_t* sb = in3_rpc_handle_start(hctx);
  sb_add_rawbytes(sb, "\"0x", data, -1);
  sb_add_char(sb, '"');
  return in3_rpc_handle_finish(hctx);
}

in3_ret_t in3_rpc_handle_with_string(in3_rpc_handle_ctx_t* hctx, char* data) {
  sb_add_chars(in3_rpc_handle_start(hctx), data);
  return in3_rpc_handle_finish(hctx);
}

in3_ret_t in3_rpc_handle_with_json(in3_rpc_handle_ctx_t* ctx, d_token_t* result) {
  if (!result) return req_set_error(ctx->req, "No result", IN3_ERPC);
  sb_t* sb = in3_rpc_handle_start(ctx);

  // As the API might return an empty string as a response,
  // we at least convert it into an empty object
  if ((d_type(result) == T_STRING || d_type(result) == T_BYTES) && d_len(result) == 0) {
    sb_add_chars(sb, "{}");
  }
  else {
    sb_add_json(sb, "", result);
  }

  // now convert all kebab-case to pascal case
  for (char* c = sb->data; *c; c++) {
    if (*c == '-' && in_property_name(c)) *c = '_';
  }
  return in3_rpc_handle_finish(ctx);
}

in3_ret_t in3_rpc_handle_with_int(in3_rpc_handle_ctx_t* hctx, uint64_t value) {
  uint8_t val[8];
  long_to_bytes(value, val);
  bytes_t b = bytes(val, 8);
  b_optimize_len(&b);
  char* s = alloca(b.len * 2 + 5);
  bytes_to_hex(b.data, b.len, s + 3);
  if (s[3] == '0') s++;
  size_t l = strlen(s + 3) + 3;
  s[0]     = '"';
  s[1]     = '0';
  s[2]     = 'x';
  s[l]     = '"';
  s[l + 1] = 0;
  return in3_rpc_handle_with_string(hctx, s);
}

static in3_ret_t req_send_sub_request_internal(in3_req_t* parent, char* method, char* params, char* in3, d_token_t** result, in3_req_t** child, bool use_cache) {
  if (params == NULL) params = "";
  char* req = NULL;
  if (use_cache) {
    req = alloca(strlen(params) + strlen(method) + 20 + (in3 ? 10 + strlen(in3) : 0));
    if (in3)
      sprintf(req, "{\"method\":\"%s\",\"params\":[%s],\"in3\":%s}", method, params, in3);
    else
      sprintf(req, "{\"method\":\"%s\",\"params\":[%s]}", method, params);
  }

  in3_req_t* ctx = parent->required;
  for (; ctx; ctx = ctx->required) {
    if (use_cache) {
      // only check first entry
      bool found = false;
      for (cache_entry_t* e = ctx->cache; e && !found; e = e->next) {
        if (e->props & CACHE_PROP_SRC_REQ) {
          if (strcmp((char*) e->value.data, req) == 0) found = true;
        }
      }
      if (found) break;
    }
    if (strcmp(d_get_string(ctx->requests[0], K_METHOD), method) != 0) continue;
    d_token_t* t = d_get(ctx->requests[0], K_PARAMS);
    if (!t) continue;
    str_range_t p = d_to_json(t);
    if (strncmp(params, p.data + 1, p.len - 2) == 0) break;
  }

  if (ctx && child) *child = ctx;

  if (ctx)
    switch (in3_req_state(ctx)) {
      case REQ_ERROR:
        return req_set_error(parent, ctx->error, ctx->verification_state ? ctx->verification_state : IN3_ERPC);
      case REQ_SUCCESS:
        *result = strcmp(method, "in3_http") == 0 ? ctx->responses[0] : d_get(ctx->responses[0], K_RESULT);
        if (!*result) {
          char* s = d_get_string(d_get(ctx->responses[0], K_ERROR), K_MESSAGE);
          UNUSED_VAR(s); // this makes sure we don't get a warning when building with _DLOGGING=false
          return req_set_error(parent, s ? s : "error executing provider call", IN3_ERPC);
        }
        return IN3_OK;
      case REQ_WAITING_TO_SEND:
      case REQ_WAITING_FOR_RESPONSE:
        return IN3_WAITING;
    }

  // create the call
  req = use_cache ? _strdupn(req, -1) : _malloc(strlen(params) + strlen(method) + 26 + (in3 ? 7 + strlen(in3) : 0));
  if (!use_cache) {
    if (in3)
      sprintf(req, "{\"method\":\"%s\",\"params\":[%s],\"in3\":%s}", method, params, in3);
    else
      sprintf(req, "{\"method\":\"%s\",\"params\":[%s]}", method, params);
  }
  ctx = req_new(parent->client, req);
  if (!ctx) {
    if (use_cache && req) _free(req);
    return req_set_error(parent, "Invalid request!", IN3_ERPC);
  }
  if (child) *child = ctx;

  // inherit cache-entries
  for (cache_entry_t* ce = parent->cache; ce; ce = ce->next) {
    if (ce->props & CACHE_PROP_INHERIT) {
      cache_entry_t* entry = in3_cache_add_entry(&ctx->cache, ce->key, ce->value);
      entry->props         = ce->props & (~CACHE_PROP_MUST_FREE);
      memcpy(entry->buffer, ce->buffer, 4);
    }
  }

  if (use_cache)
    in3_cache_add_ptr(&ctx->cache, req)->props = CACHE_PROP_SRC_REQ;
  in3_ret_t ret = req_add_required(parent, ctx);
  if (ret == IN3_OK && ctx->responses[0]) {
    *result = d_get(ctx->responses[0], K_RESULT);
    if (!*result) {
      char* s = d_get_string(d_get(ctx->responses[0], K_ERROR), K_MESSAGE);
      UNUSED_VAR(s); // this makes sure we don't get a warning when building with _DLOGGING=false
      return req_set_error(parent, s ? s : "error executing provider call", IN3_ERPC);
    }
  }
  return ret;
}

in3_ret_t req_send_sub_request(in3_req_t* parent, char* method, char* params, char* in3, d_token_t** result, in3_req_t** child) {
  bool use_cache = strcmp(method, "eth_sendTransaction") == 0;
  return req_send_sub_request_internal(parent, method, params, in3, result, child, use_cache);
}

in3_ret_t req_send_id_sub_request(in3_req_t* parent, char* method, char* params, char* in3, d_token_t** result, in3_req_t** child) {
  return req_send_sub_request_internal(parent, method, params, in3, result, child, true);
}

static inline const char* method_for_sigtype(d_digest_type_t type) {
  switch (type) {
    case SIGN_EC_RAW: return "sign_ec_raw";
    case SIGN_EC_HASH: return "sign_ec_hash";
    case SIGN_EC_PREFIX: return "sign_ec_prefix";
    case SIGN_EC_BTC: return "sign_ec_btc";
    default: return "sign_ec_hash";
  }
}

in3_ret_t req_send_sign_request(in3_req_t* ctx, d_digest_type_t type, d_curve_type_t curve_type, d_payload_type_t pl_type, bytes_t* signature, bytes_t raw_data, bytes_t from, d_token_t* meta, bytes_t cache_key) {

  bytes_t* cached_sig = in3_cache_get_entry(ctx->cache, &cache_key);
  if (cached_sig) {
    *signature = *cached_sig;
    return IN3_OK;
  }

  // get the signature from required
  const char* method = method_for_sigtype(type);
  sb_t        params = {0};
  sb_add_bytes(&params, "[", &raw_data, 1, false);
  sb_add_chars(&params, ",");
  sb_add_bytes(&params, NULL, &from, 1, false);
  sb_add_chars(&params, ",");
  sb_add_int(&params, (int64_t) pl_type);
  sb_add_chars(&params, ",");
  sb_add_int(&params, (int64_t) curve_type);
  sb_add_json(&params, ",", meta);
  sb_add_chars(&params, "]");

  in3_req_t* c = req_find_required(ctx, method, params.data);
  if (c) {
    _free(params.data);
    switch (in3_req_state(c)) {
      case REQ_ERROR:
        return req_set_error(ctx, c->error ? c->error : "Could not handle signing", IN3_ERPC);
      case REQ_WAITING_FOR_RESPONSE:
      case REQ_WAITING_TO_SEND:
        return IN3_WAITING;
      case REQ_SUCCESS: {
        if (c->raw_response && c->raw_response->state == IN3_OK && c->raw_response->data.len > 64) {
          *signature = cloned_bytes(bytes((uint8_t*) c->raw_response->data.data, c->raw_response->data.len));
          in3_cache_add_entry(&ctx->cache, cloned_bytes(cache_key), *signature);
          req_remove_required(ctx, c, false);
          return IN3_OK;
        }
        else if (c->raw_response && c->raw_response->state)
          return req_set_error(ctx, c->raw_response->data.data, c->raw_response->state);
        else
          return req_set_error(ctx, "no data to sign", IN3_EINVAL);
        default:
          return req_set_error(ctx, "invalid state", IN3_EINVAL);
      }
    }
  }
  else {
    sb_t req = {0};
    sb_add_chars(&req, "{\"method\":\"");
    sb_add_chars(&req, method);
    sb_add_chars(&req, "\",\"params\":");
    sb_add_chars(&req, params.data);
    sb_add_chars(&req, "}");
    _free(params.data);
    c = req_new(ctx->client, req.data);
    if (!c) return IN3_ECONFIG;
    c->type = RT_SIGN;
    return req_add_required(ctx, c);
  }
}

in3_ret_t req_require_signature(in3_req_t* ctx, d_digest_type_t digest_type, d_curve_type_t curve_type, d_payload_type_t pl_type, bytes_t* signature, bytes_t raw_data, bytes_t from, d_token_t* meta) {
  bytes_t cache_key = bytes(alloca(raw_data.len + from.len), raw_data.len + from.len);
  memcpy(cache_key.data, raw_data.data, raw_data.len);
  if (from.data) memcpy(cache_key.data + raw_data.len, from.data, from.len);
  bytes_t* cached_sig = in3_cache_get_entry(ctx->cache, &cache_key);
  if (cached_sig) {
    *signature = *cached_sig;
    return IN3_OK;
  }

  in3_log_debug("requesting signature type=%d from account %x\n", digest_type, from.len > 2 ? bytes_to_int(from.data, 4) : 0);

  // first try internal plugins for signing, before we create an context.
  if (in3_plugin_is_registered(ctx->client, PLGN_ACT_SIGN)) {
    in3_sign_ctx_t sc = {.account = from, .req = ctx, .message = raw_data, .signature = NULL_BYTES, .digest_type = digest_type, .payload_type = pl_type, .meta = meta, .curve_type = curve_type};
    in3_ret_t      r  = in3_plugin_execute_first_or_none(ctx, PLGN_ACT_SIGN, &sc);
    if (r == IN3_OK && sc.signature.data) {
      in3_cache_add_entry(&ctx->cache, cloned_bytes(cache_key), sc.signature);
      *signature = sc.signature;
      return IN3_OK;
    }
    else if (r != IN3_EIGNORE && r != IN3_OK)
      return r;
  }
  in3_log_debug("nobody picked up the signature, sending req now \n");
  return req_send_sign_request(ctx, digest_type, curve_type, pl_type, signature, raw_data, from, meta, cache_key);
}

in3_ret_t vc_set_error(in3_vctx_t* vc, char* msg) {
#ifdef LOGGING
  (void) req_set_error(vc->req, msg, IN3_EUNKNOWN);
#else
  (void) msg;
  (void) vc;
#endif
  return IN3_EUNKNOWN;
}
