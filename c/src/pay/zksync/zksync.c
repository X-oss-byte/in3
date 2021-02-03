/*******************************************************************************
 * This file is part of the Incubed project.
 * Sources: https://github.com/slockit/in3-c
 * 
 * Copyright (C) 2018-2019 slock.it GmbH, Blockchains LLC
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
#include "zksync.h"
#include "../../core/client/context_internal.h"
#include "../../core/client/keys.h"
#include "../../core/util/debug.h"
#include "../../third-party/zkcrypto/lib.h"
#include "zk_helper.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static zk_sign_type_t get_sign_type(d_token_t* type) {
  if (type == NULL) return ZK_SIGN_PK;
  char* c = d_string(type);
  if (strcmp(c, "contract") == 0) return ZK_SIGN_CONTRACT;
  if (strcmp(c, "create2") == 0) return ZK_SIGN_CREATE2;
  return ZK_SIGN_PK;
}

static in3_ret_t ensure_provider(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  if (conf->provider_url) return IN3_OK;
  switch (ctx->ctx->client->chain.chain_id) {
    case CHAIN_ID_MAINNET:
      conf->provider_url = _strdupn("https://api.zksync.io/jsrpc", -1);
      break;
    default:
      return ctx_set_error(ctx->ctx, "no provider_url in config", IN3_EINVAL);
  }
  return IN3_OK;
}

static in3_ret_t zksync_get_key(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  bytes32_t k;
  TRY(zksync_get_sync_key(conf, ctx->ctx, k))
  return in3_rpc_handle_with_bytes(ctx, bytes(k, 32));
}

static in3_ret_t zksync_get_pubkeyhash(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx, d_token_t* params) {
  address_t pubkey_hash;
  if (d_len(params) == 1) {
    CHECK_PARAM_TYPE(ctx->ctx, params, 0, T_BYTES)
    CHECK_PARAM_LEN(ctx->ctx, params, 0, 32)
    TRY(zkcrypto_pubkey_hash(d_to_bytes(params + 1), pubkey_hash));
  }
  else
    TRY(zksync_get_pubkey_hash(conf, ctx->ctx, pubkey_hash))
  char res[48];
  strcpy(res, "\"sync:");
  bytes_to_hex(pubkey_hash, 20, res + 6);
  strcpy(res + 46, "\"");
  return in3_rpc_handle_with_string(ctx, res);
}

static in3_ret_t zksync_get_pubkey(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  bytes32_t pubkey;
  if (conf->musig_pub_keys.data)
    TRY(zkcrypto_compute_aggregated_pubkey(conf->musig_pub_keys, pubkey))
  else if (!memiszero(conf->pub_key, 32))
    memcpy(pubkey, conf->pub_key, 32);
  else {
    bytes32_t k;
    TRY(zksync_get_sync_key(conf, ctx->ctx, k))
    TRY(zkcrypto_pk_to_pubkey(k, pubkey))
    memcpy(conf->pub_key, pubkey, 32);
  }
  return in3_rpc_handle_with_bytes(ctx, bytes(pubkey, 32));
}

static in3_ret_t zksync_aggregate_pubkey(in3_rpc_handle_ctx_t* ctx, d_token_t* params) {
  bytes32_t dst;
  CHECK_PARAM_TYPE(ctx->ctx, params, 0, T_BYTES)
  CHECK_PARAM(ctx->ctx, params, 0, d_len(val) % 32 == 0)

  TRY(zkcrypto_compute_aggregated_pubkey(d_to_bytes(params + 1), dst))
  return in3_rpc_handle_with_bytes(ctx, bytes(dst, 32));
}

static in3_ret_t zksync_account_address(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  uint8_t* account = NULL;
  TRY(zksync_get_account(conf, ctx->ctx, &account));
  return in3_rpc_handle_with_bytes(ctx, bytes(account, 20));
}

static in3_ret_t zksync_contract_address(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  uint8_t* adr;
  TRY(zksync_get_contracts(conf, ctx->ctx, &adr))
  sb_t* sb = in3_rpc_handle_start(ctx);
  sb_add_rawbytes(sb, "{\"govContract\":\"0x", bytes(conf->gov_contract, 20), 0);
  sb_add_rawbytes(sb, "\",\"mainContract\":\"0x", bytes(conf->main_contract, 20), 0);
  sb_add_chars(sb, "\"}");
  return in3_rpc_handle_finish(ctx);
}
static in3_ret_t zksync_tokens(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  TRY(resolve_tokens(conf, ctx->ctx, NULL, NULL))
  sb_t* sb = in3_rpc_handle_start(ctx);
  sb_add_char(sb, '{');
  for (unsigned int i = 0; i < conf->token_len; i++) {
    if (i) sb_add_char(sb, ',');
    sb_add_char(sb, '\"');
    sb_add_chars(sb, conf->tokens[i].symbol);
    sb_add_rawbytes(sb, "\":{\"address\":\"0x", bytes(conf->tokens[i].address, 20), 0);
    sb_add_chars(sb, "\",\"decimals\":");
    sb_add_int(sb, conf->tokens[i].decimals);
    sb_add_chars(sb, ",\"id\":");
    sb_add_int(sb, conf->tokens[i].id);
    sb_add_chars(sb, ",\"symbol\":\"");
    sb_add_chars(sb, conf->tokens[i].symbol);
    sb_add_chars(sb, "\"}");
  }
  sb_add_char(sb, '}');
  return in3_rpc_handle_finish(ctx);
}

// --- handle rpc----
static in3_ret_t zksync_rpc(zksync_config_t* conf, in3_rpc_handle_ctx_t* ctx) {
  char*      method = d_get_stringk(ctx->ctx->requests[0], K_METHOD);
  d_token_t* params = d_get(ctx->ctx->requests[0], K_PARAMS);

  // check the prefix (zksync_ or zk_ is supported)
  if (strncmp(method, "zksync_", 7) == 0)
    method += 7;
  else if (strncmp(method, "zk_", 3) == 0)
    method += 3;
  else
    return IN3_EIGNORE;

  // make sure the provider is set
  TRY(ensure_provider(conf, ctx))

  // handle rpc -functions
  TRY_RPC("deposit", zksync_deposit(conf, ctx, params))
  TRY_RPC("transfer", zksync_transfer(conf, ctx, params, ZK_TRANSFER))
  TRY_RPC("withdraw", zksync_transfer(conf, ctx, params, ZK_WITHDRAW))
  TRY_RPC("set_key", zksync_set_key(conf, ctx, params))
  TRY_RPC("emergency_withdraw", zksync_emergency_withdraw(conf, ctx, params))
  TRY_RPC("sync_key", zksync_get_key(conf, ctx))
  TRY_RPC("aggregate_pubkey", zksync_aggregate_pubkey(ctx, params))
  TRY_RPC("pubkeyhash", zksync_get_pubkeyhash(conf, ctx, params))
  TRY_RPC("pubkey", zksync_get_pubkey(conf, ctx))
  TRY_RPC("account_address", zksync_account_address(conf, ctx))
  TRY_RPC("contract_address", zksync_contract_address(conf, ctx))
  TRY_RPC("tokens", zksync_tokens(conf, ctx))
  TRY_RPC("sign", zksync_musig_sign(conf, ctx, params))
  TRY_RPC("verify", in3_rpc_handle_with_int(ctx, conf->musig_pub_keys.data
                                                     ? zkcrypto_verify_signatures(d_to_bytes(params + 1), conf->musig_pub_keys, d_to_bytes(params + 2))
                                                     : zkcrypto_verify_musig(d_to_bytes(params + 1), d_to_bytes(params + 2))))

  // prepare fallback to send to zksync-server
  str_range_t p            = d_to_json(params);
  char*       param_string = alloca(p.len - 1);
  memcpy(param_string, p.data + 1, p.len - 2);
  param_string[p.len - 2] = 0;

  if (strcmp(method, "account_info") == 0) {
    if (*param_string == 0) {
      TRY(zksync_get_account(conf, ctx->ctx, NULL))
      param_string = alloca(45);
      set_quoted_address(param_string, conf->account);
    }
    else
      CHECK_PARAM_ADDRESS(ctx->ctx, params, 0)
  }

  // we need to show the arguments as integers
  if (strcmp(method, "ethop_info") == 0)
    sprintf(param_string, "%i", d_get_int_at(params, 0));

  // send request to the server
  d_token_t* result;
  TRY(send_provider_request(ctx->ctx, conf, method, param_string, &result))

  // format result
  char* json = d_create_json(NULL, result);
  in3_rpc_handle_with_string(ctx, json);
  _free(json);
  return IN3_OK;
}

static in3_ret_t config_free(zksync_config_t* conf) {
  if (conf->musig_urls) {
    for (unsigned int i = 0; i < conf->musig_pub_keys.len / 32; i++) {
      if (conf->musig_urls[i]) _free(conf->musig_urls[i]);
    }
    _free(conf->musig_urls);
  }
  if (conf->provider_url) _free(conf->provider_url);
  if (conf->main_contract) _free(conf->main_contract);
  if (conf->account) _free(conf->account);
  if (conf->tokens) _free(conf->tokens);
  if (conf->create2) _free(conf->create2);
  if (conf->musig_pub_keys.data) _free(conf->musig_pub_keys.data);
  while (conf->musig_sessions) conf->musig_sessions = zk_musig_session_free(conf->musig_sessions);
  _free(conf);
  return IN3_OK;
}

static in3_ret_t config_get(zksync_config_t* conf, in3_get_config_ctx_t* ctx) {
  sb_add_chars(ctx->sb, ",\"zksync\":{\"provider_url\":\"");
  sb_add_chars(ctx->sb, conf->provider_url ? conf->provider_url : "");
  sb_add_char(ctx->sb, '\"');
  if (conf->account) {
    bytes_t ac = bytes(conf->account, 20);
    sb_add_bytes(ctx->sb, ",\"account\"=", &ac, 1, false);
  }
  sb_add_chars(ctx->sb, ",\"signer_type\":\"");
  if (conf->sign_type == ZK_SIGN_CONTRACT)
    sb_add_chars(ctx->sb, "contract\"");
  else if (conf->sign_type == ZK_SIGN_CREATE2)
    sb_add_chars(ctx->sb, "create2\"");
  else
    sb_add_chars(ctx->sb, "pk\"");
  sb_add_char(ctx->sb, '}');
  return IN3_OK;
}

static in3_ret_t config_set(zksync_config_t* conf, in3_configure_ctx_t* ctx) {
  if (ctx->token->key != key("zksync")) return IN3_EIGNORE;

  const char* provider = d_get_string(ctx->token, "provider_url");
  if (provider) conf->provider_url = _strdupn(provider, -1);
  bytes_t* account = d_get_bytes(ctx->token, "account");
  if (account && account->len == 20) memcpy(conf->account = _malloc(20), account->data, 20);
  bytes_t sync_key = d_to_bytes(d_get(ctx->token, key("sync_key")));
  if (sync_key.len) {
    zkcrypto_pk_from_seed(sync_key, conf->sync_key);
    zkcrypto_pk_to_pubkey(conf->sync_key, conf->pub_key);
    zkcrypto_pubkey_hash(bytes(conf->pub_key, 32), conf->pub_key_hash_pk);
  }

  bytes_t* main_contract = d_get_bytes(ctx->token, "main_contract");
  if (main_contract && main_contract->len == 20) memcpy(conf->main_contract = _malloc(20), main_contract->data, 20);
  d_token_t* st = d_get(ctx->token, key("signer_type"));
  if (st)
    conf->sign_type = get_sign_type(st);
  else if (conf->sign_type == 0)
    conf->sign_type = ZK_SIGN_PK;
  conf->version    = (uint32_t) d_intd(d_get(ctx->token, key("version")), conf->version);
  d_token_t* musig = d_get(ctx->token, key("musig_pub_keys"));
  if (musig && d_type(musig) == T_BYTES && d_len(musig) % 32 == 0) {
    if (conf->musig_pub_keys.data) _free(conf->musig_pub_keys.data);
    conf->musig_pub_keys = bytes(_malloc(d_len(musig)), musig->len);
    memcpy(conf->musig_pub_keys.data, musig->data, musig->len);
  }
  d_token_t* urls = d_get(ctx->token, key("musig_urls"));
  if (urls) {
    if (conf->musig_urls) {
      for (unsigned int i = 0; i < conf->musig_pub_keys.len / 32; i++) {
        if (conf->musig_urls[i]) _free(conf->musig_urls[i]);
      }
      _free(conf->musig_urls);
    }
    conf->musig_urls = _calloc(d_len(urls), sizeof(char*));
    for (int i = 0; i < d_len(urls); i++) {
      char* s = d_get_string_at(urls, i);
      if (s) conf->musig_urls[i] = _strdupn(s, -1);
    }
  }
  d_token_t* create2 = d_get(ctx->token, key("create2"));
  if (create2) {
    conf->sign_type = ZK_SIGN_CREATE2;
    if (!conf->create2) conf->create2 = _calloc(1, sizeof(zk_create2_t));
    bytes_t* t = d_get_bytesk(create2, key("creator"));
    if (t && t->len == 20) memcpy(conf->create2->creator, t->data, 20);
    t = d_get_bytesk(create2, key("saltarg"));
    if (t && t->len == 32) memcpy(conf->create2->salt_arg, t->data, 32);
    t = d_get_bytesk(create2, key("codehash"));
    if (t && t->len == 32) memcpy(conf->create2->codehash, t->data, 32);
  }
  return IN3_OK;
}

static in3_ret_t handle_zksync(void* conf, in3_plugin_act_t action, void* arg) {
  switch (action) {
    case PLGN_ACT_TERM: return config_free(conf);
    case PLGN_ACT_CONFIG_GET: return config_get(conf, arg);
    case PLGN_ACT_CONFIG_SET: return config_set(conf, arg);
    case PLGN_ACT_RPC_HANDLE: return zksync_rpc(conf, arg);
    default: return IN3_ENOTSUP;
  }
  return IN3_EIGNORE;
}

in3_ret_t in3_register_zksync(in3_t* c) {
  zksync_config_t* conf = _calloc(sizeof(zksync_config_t), 1);
  conf->version         = 1;
  return in3_plugin_register(c, PLGN_ACT_RPC_HANDLE | PLGN_ACT_TERM | PLGN_ACT_CONFIG_GET | PLGN_ACT_CONFIG_SET, handle_zksync, conf, false);
}