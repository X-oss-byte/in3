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

#ifndef TEST
#define TEST
#endif
#ifndef TEST
#define DEBUG
#endif

#include "../../src/core/client/cache.h"
#include "../../src/core/client/context.h"
#include "../../src/core/client/keys.h"
#include "../../src/core/client/nodelist.h"
#include "../../src/core/util/data.h"
#include "../../src/core/util/utils.h"
#include "../../src/verifier/eth1/basic/eth_basic.h"
#include "../test_utils.h"
#include <stdio.h>
#include <unistd.h>

void test_configure_request() {
  in3_register_eth_basic();

  in3_t* c               = in3_new();
  c->proof               = PROOF_FULL;
  c->signatureCount      = 2;
  c->chains->needsUpdate = false;
  c->finality            = 10;
  c->includeCode         = true;
  c->replaceLatestBlock  = 6;
  c->use_binary          = true;
  c->use_http            = true;

  in3_ctx_t* ctx = new_ctx(c, "{\"method\":\"eth_getBlockByNumber\",\"params\":[\"latest\",false]}");
  TEST_ASSERT_EQUAL(IN3_WAITING, in3_ctx_execute(ctx));
  in3_request_t* request = in3_create_request(ctx);
  json_ctx_t*    json    = parse_json(request->payload);
  d_token_t*     in3     = d_get(d_get_at(json->result, 0), K_IN3);
  TEST_ASSERT_NOT_NULL(in3);
  TEST_ASSERT_EQUAL(1, d_get_int(in3, "useFullProof"));
  TEST_ASSERT_EQUAL(1, d_get_int(in3, "useBinary"));
  TEST_ASSERT_EQUAL(10, d_get_int(in3, "finality"));
  TEST_ASSERT_EQUAL(6, d_get_int(in3, "latestBlock"));
  d_token_t* signers = d_get(in3, key("signers"));
  TEST_ASSERT_NOT_NULL(signers);
  TEST_ASSERT_EQUAL(2, d_len(signers));
  free_request(request, ctx, false);
  free_json(json);
  free_ctx(ctx);
  in3_free(c);
}

/*
 * Main
 */
int main() {
  TESTS_BEGIN();
  RUN_TEST(test_configure_request);
  return TESTS_END();
}
