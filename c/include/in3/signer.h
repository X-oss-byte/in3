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

// @PUBLIC_HEADER
/** @file
 * Ethereum Nano verification.
 * */

#ifndef in3_signer_h__
#define in3_signer_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include "plugin.h"

/**
 * simply signer with one private key.
 *
 * since the pk pointting to the 32 byte private key is not cloned, please make sure, you manage memory allocation correctly!
 */
in3_ret_t eth_set_pk_signer(in3_t* in3, bytes32_t pk, d_curve_type_t type);

/**
 * registers pk signer as plugin so you can use config or in3_addKeys as rpc
 */
in3_ret_t eth_register_pk_signer(in3_t* in3);

/** sets the signer and a pk to the client*/
in3_ret_t eth_set_request_signer(in3_t* in3, bytes32_t pk);

/**
 * simply signer with one private key as hex.
 */
void eth_set_pk_signer_hex(in3_t* in3, char* key);

/** Signs message after hashing it with hasher function given in 'hasher_t', with the given private key*/
in3_ret_t ec_sign_pk_hash(uint8_t* message, size_t len, uint8_t* pk, d_digest_type_t hasher, uint8_t* dst);

/** hashes the msg by adding the Ethereum Signed Message-Prefix */
void eth_create_prefixed_msg_hash(bytes32_t dst, bytes_t msg);

/** signs with a pk bases on the type */
bytes_t sign_with_pk(const bytes32_t pk, const bytes_t data, const d_digest_type_t type);
#ifdef __cplusplus
}
#endif

#endif
