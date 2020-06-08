//! Bitcoin JSON RPC client API.
use core::convert;
use std::convert::TryInto;
use std::ffi::CString;

use ethereum_types::U256;
use serde::Deserialize;
use serde_json::{json, Value};

use crate::error::In3Result;
use crate::eth1::Hash;
use crate::json_rpc::{Request, rpc};
use crate::traits::{Api as ApiTrait, Client as ClientTrait};
use crate::types::Bytes;

#[derive(Debug, Deserialize)]
pub struct TransactionInput {
    vout: u32,
    txid: U256,
    sequence: u32,
    script: Bytes,
    txinwitness: Bytes,
}

#[derive(Debug, Deserialize)]
pub struct TransactionOutput {
    value: u64,
    n: u32,
    script_pubkey: Bytes,
}

#[derive(Debug, Deserialize)]
pub struct Transaction {
    in_active_chain: bool,
    data: Bytes,
    txid: U256,
    hash: Hash,
    size: u32,
    vsize: u32,
    weight: u32,
    version: u32,
    locktime: u32,
    vin: Vec<TransactionInput>,
    vout: Vec<TransactionOutput>,
    blockhash: Hash,
    confirmations: u32,
    time: u32,
    blocktime: u32,
}

pub struct BlockHeader {
    hash: Hash,
    confirmations: u32,
    height: u32,
    version: u32,
    merkleroot: Hash,
    time: u32,
    nonce: u32,
    bits: [u8; 4],
    chainwork: U256,
    n_tx: u32,
    previous_hash: Hash,
    next_hash: Hash,
    data: [u8; 80],
}

impl convert::From<BlockHeaderSerdeable> for BlockHeader {
    fn from(header: BlockHeaderSerdeable) -> Self {
        BlockHeader {
            hash: Hash::from_slice(header.hash.0.as_slice()),
            confirmations: header.confirmations,
            height: header.height,
            version: header.version,
            merkleroot: Hash::from_slice(header.merkleroot.0.as_slice().into()),
            time: header.time,
            nonce: header.nonce,
            bits: header.bits.0.as_slice().try_into().expect("incorrect bits len"),
            chainwork: header.chainwork.0.as_slice().into(),
            n_tx: header.n_tx,
            previous_hash: Hash::from_slice(header.previous_hash.0.as_slice().into()),
            next_hash: Hash::from_slice(header.next_hash.0.as_slice().into()),
            data: [0; 80],
        }
    }
}


#[derive(Deserialize)]
struct BlockHeaderSerdeable {
    hash: Bytes,
    confirmations: u32,
    height: u32,
    version: u32,
    merkleroot: Bytes,
    time: u32,
    nonce: u32,
    bits: Bytes,
    chainwork: Bytes,
    #[serde(rename = "nTx")]
    n_tx: u32,
    #[serde(rename = "previousblockhash")]
    previous_hash: Bytes,
    #[serde(rename = "nextblockhash")]
    next_hash: Bytes,
}


#[derive(Deserialize)]
#[serde(untagged)]
pub enum BlockTransactions {
    Hashes(Vec<Hash>),
    Transactions(Vec<Transaction>),
}

pub struct Block {
    header: BlockHeader,
    transactions: BlockTransactions,
}


pub struct Api {
    client: Box<dyn ClientTrait>,
}

impl ApiTrait for Api {
    fn new(client: Box<dyn ClientTrait>) -> Self {
        Api { client }
    }

    fn client(&mut self) -> &mut Box<dyn ClientTrait> {
        &mut self.client
    }
}

impl Api {
    pub async fn get_blockheader(&mut self, blockhash: Hash) -> In3Result<BlockHeader> {
        let hash = json!(blockhash);
        let hash_str = hash.as_str().unwrap();
        let header: Value = rpc(self.client(), Request {
            method: "getblockheader",
            params: json!([hash_str.trim_start_matches("0x"), true]),
        }).await?;
        let data = unsafe {
            let mut data = [0u8; 80];
            let js = CString::new(header.to_string()).expect("CString::new failed");
            let j_data = in3_sys::parse_json(js.as_ptr());
            let _ = in3_sys::btc_serialize_block_header((*j_data).result, data.as_mut_ptr());
            in3_sys::json_free(j_data);
            data
        };
        let mut header: BlockHeader = serde_json::from_str::<BlockHeaderSerdeable>(header.to_string().as_str())?.into();
        header.data = data;
        Ok(header)
    }

    pub async fn get_transaction_bytes(&mut self, tx_id: Hash) -> In3Result<Bytes> {
        let hash = json!(tx_id);
        let hash_str = hash.as_str().unwrap();
        rpc(self.client(), Request {
            method: "getrawtransaction",
            params: json!([hash_str.trim_start_matches("0x"), false]),
        }).await
    }
}


#[cfg(test)]
mod tests {
    use async_std::task;
    use rustc_hex::FromHex;

    use crate::prelude::*;

    use super::*;

    #[test]
    fn test_btc_get_blockheader() -> In3Result<()> {
        let mut api = Api::new(Client::new(chain::BTC));
        api.client
            .configure(r#"{"autoUpdateList":false,"nodes":{"0x99":{"needsUpdate":false}}}}"#)?;
        api.client.set_transport(Box::new(MockTransport {
            responses: vec![(
                "getblockheader",
                r#"[{
                    "id": 1,
                    "jsonrpc": "2.0",
                    "result": {
                        "hash": "00000000000000000007171457f3352e101d92bca75f055c330fe33e84bb183b",
                        "confirmations": 1979,
                        "height": 631076,
                        "version": 536870912,
                        "versionHex": "20000000",
                        "merkleroot": "18f5756c66b14aa8bace933001b16d78bd89a16612468122442559ce9f296eb6",
                        "time": 1589991753,
                        "mediantime": 1589989643,
                        "nonce": 2921687714,
                        "bits": "171297f6",
                        "difficulty": 15138043247082.88,
                        "chainwork": "00000000000000000000000000000000000000000f9f574f8d39680a92ad1bdc",
                        "nTx": 2339,
                        "previousblockhash": "000000000000000000061e12d6a29bd0175a6045dfffeafd950c0513f9b82c80",
                        "nextblockhash": "00000000000000000000eac6e799c468b3a140d9e1400c31f7603fdb20e1198d"
                    },
                    "in3": {
                        "proof": {
                            "cbtx": "0x010000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff470324a109fabe6d6d565bf669fbb3f2c3176649a805295685fbb0f317830d3eb7a94ab32438815a3001000000000000002865060003c9f66f712b004a9e9791052f736c7573682f00000000030f98ee31000000001976a9147c154ed1dc59609e3d26abb2df2ea3d587cd8c4188ac00000000000000002c6a4c2952534b424c4f434b3a85a796712f0d9110ed0c6c7056bf368c08f790f5e6a7a901590bd1210024303b0000000000000000266a24aa21a9ed4efc104c06ebc2d2033b0511518c2e87d6841e7716c4f37c82d54819838b491e0120000000000000000000000000000000000000000000000000000000000000000000000000",
                            "cbtxMerkleProof": "0xc8ec6a0f18df96add7d9b2bdc627cf4e3c64b5c14466494487065a360bc614eebe66ba1702aa7a0eef97146f5b0fd544dad9bad33125187bc1933853f94dd54a176baa3181b168eb6eca2dc7953d167ff5b12beec84baa6ca89b80341e329f96db85db46909d0a773570895828df2cf30122d85e54a52e8b0a96f75cfe15d75412e904251b18b31903954fdd7c609c60e5564d563853cdf0d59f4ade9e19bb3b0e6f1dda2ae3d63fc8e04892e75f989656e7c8823584cb1cd59a857bd206d684d83b599702a6078ad9bd2cdeea597445f5f02e054fcf9a049d4ee0e6c07b4ef5b07ca72a1be41bfb2b800b77255755c0fa7280582c48f7e4352a662e05dc787aefbbf1978227ce6933be11f6edafcf41b8fcc927915722f22fd9650190c46a84d966f4b79c89f4d8c858ebfc42d4431c1d8877dac16c2d9cecee6f642d84ca437c4fe68d2020d078c1b9bfcc07ad370dc85b19e59666f40ba3a5a3006e667093751032f35590137e0c77d99b6edca34b8dbc8413d4df3842d3908ae4dfb8adcf"
                        },
                        "lastNodeList": 2809569,
                        "execTime": 95,
                        "rpcTime": 0,
                        "rpcCount": 0,
                        "currentBlock": 2816454,
                        "version": "2.1.0"
                    }
                }]"#,
            )],
        }));
        let header = task::block_on(
            api.get_blockheader(serde_json::from_str::<Hash>(r#""0x00000000000000000007171457f3352e101d92bca75f055c330fe33e84bb183b""#)?)
        ).unwrap();
        assert_eq!(header.confirmations, 1979);
        assert_eq!(header.height, 631076);
        assert_eq!(header.version, 536870912);
        assert_eq!(header.chainwork, serde_json::from_str::<U256>(r#""0x00000000000000000000000000000000000000000f9f574f8d39680a92ad1bdc""#)?);
        assert_eq!(header.n_tx, 2339);
        assert_eq!(header.next_hash, serde_json::from_str::<Hash>(r#""0x00000000000000000000eac6e799c468b3a140d9e1400c31f7603fdb20e1198d""#)?);
        // it is sufficient to verify data field as it contains all remaining fields serialized
        assert_eq!(header.data.to_vec(), FromHex::from_hex("00000020802cb8f913050c95fdeaffdf45605a17d09ba2d6121e06000000000000000000b66e299fce5925442281461266a189bd786db1013093cebaa84ab1666c75f5184959c55ef6971217a26a25ae").unwrap());
        Ok(())
    }

    #[test]
    fn test_btc_get_transaction_bytes() -> In3Result<()> {
        let mut api = Api::new(Client::new(chain::BTC));
        api.client
            .configure(r#"{"autoUpdateList":false,"nodes":{"0x99":{"needsUpdate":false}}}}"#)?;
        api.client.set_transport(Box::new(MockTransport {
            responses: vec![(
                "getrawtransaction",
                r#"[{
                    "id": 1,
                    "jsonrpc": "2.0",
                    "result": "01000000000101dccee3ce73ba66bc2d2602d647e1238a76d795cfb120f520ba64b0f085e2f694010000001716001430d71be06aa53fd845913f8613ed518d742d082affffffff02c0d8a7000000000017a914d129842dbe1ee73e69d14d54a8a62784877fb83e87108428030000000017a914e483fe5491d8ef5acf043fac5eb1af0f049a80318702473044022035c13c5fdf5f5d07c2101176db8a9c727cec9c31c612b15ae0a4cbdeb25b4dc2022046849e039477aa67fb60e24635668ae1de0bddb9ade3eac2d5ca350898d43c2b01210344715d54ec59240a4ae9f5d8e469f3933a7b03d5c09e15ac3ff53239ea1041b800000000",
                    "in3": {
                        "proof": {
                            "block": "0x00000020d5e0dfa4c490770bfbc05ae1580fede609bb96526c6e0900000000000000000059dee7066dcb0e6cc5cc2f67318223711a3109e1ae40e1927274cbecdcc446614665c35e397a1117c9c59ca6",
                            "txIndex": 2,
                            "merkleProof": "0xf4936452dbc67e5e4d7f77bc2007124daa732fcdc657834febaba45e503af2c6ba50780c54289219f6116d4616dbc95edc2651d5007f404b4ec2e27fcbbe8507e0c0be5dc87a04d7bb4808db2d16012ca3a58b6bd7bfc3f2228dd7bb9c96903bc4a19f72c59ab1932d6e8b0e1d0028eedc4e06ef8776df8e9dc913e8014349bad47515c8a3bec6c4a8affa4c2685a52955c4ba9f60c59324252d44659eb46efb1c8572f8119f71f47a83e315cf0290c4277667cd949209d5559b5df80a81eda70aeef1ad99c6352d75ad78d61f5efe5580c26312cada1eecb1006d3e8c53ff9cc3247ceeb5e6d501588d3cd416f7b7d8314146cb923d9c2de8ed79673e7225604aba67715d01b1528fb41af89a3d86d3901c0c88a6b7f5aac4ea1261c8d6bd27dea5084622ce16f984404c8e3d986a26d8e66678be413971be11ba1ec0b14788e4d2f739558ac868f33094ba8b9039e9f237daa4c4c75be3e22c851f6157afa5d8bff222d3e8f4107a800e4c5457ca5dfcc7db003e81bd57226cb6a513657337",
                            "cbtx": "0x010000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff540375a0091b4d696e656420627920416e74506f6f6c33374e00510020e66931fafabe6d6d47270091cad6f755eca76a7e99ca8a188ded9acd9a77792a500f970861181d8c04000000000000002c60120009960400ffffffff0333c34a2c000000001976a91411dbe48cc6b617f9c6adaf4d9ed5f625b1c7cb5988ac0000000000000000266a24aa21a9ede80a44d4c35adf025f0b64410ec90738f75b29f0582e388132795a603f056d780000000000000000266a24b9e11b6db66f781caca1734d3713e4fc36f31d62e041911055808d2f00fe6feb7207b9c70120000000000000000000000000000000000000000000000000000000000000000000000000",
                            "cbtxMerkleProof": "0xa5bafc1cc734e0ac3ae51c6e97f3baf69c379a7428f3493597501be74590ab67cbafa7b91b8f0a3c037cef13a587face342a6b054f5555bb98cf2ee055c97fb6e0c0be5dc87a04d7bb4808db2d16012ca3a58b6bd7bfc3f2228dd7bb9c96903bc4a19f72c59ab1932d6e8b0e1d0028eedc4e06ef8776df8e9dc913e8014349bad47515c8a3bec6c4a8affa4c2685a52955c4ba9f60c59324252d44659eb46efb1c8572f8119f71f47a83e315cf0290c4277667cd949209d5559b5df80a81eda70aeef1ad99c6352d75ad78d61f5efe5580c26312cada1eecb1006d3e8c53ff9cc3247ceeb5e6d501588d3cd416f7b7d8314146cb923d9c2de8ed79673e7225604aba67715d01b1528fb41af89a3d86d3901c0c88a6b7f5aac4ea1261c8d6bd27dea5084622ce16f984404c8e3d986a26d8e66678be413971be11ba1ec0b14788e4d2f739558ac868f33094ba8b9039e9f237daa4c4c75be3e22c851f6157afa5d8bff222d3e8f4107a800e4c5457ca5dfcc7db003e81bd57226cb6a513657337"
                        },
                        "lastNodeList": 2836487,
                        "execTime": 867,
                        "rpcTime": 183,
                        "rpcCount": 1,
                        "currentBlock": 2840357,
                        "version": "2.1.0"
                    }
                }]"#,
            )],
        }));
        let tx = task::block_on(
            api.get_transaction_bytes(serde_json::from_str::<Hash>(r#""0x83ce5041679c75721ec7135e0ebeeae52636cfcb4844dbdccf86644df88da8c1""#)?)
        ).unwrap();
        assert_eq!(tx.0, FromHex::from_hex("01000000000101dccee3ce73ba66bc2d2602d647e1238a76d795cfb120f520ba64b0f085e2f694010000001716001430d71be06aa53fd845913f8613ed518d742d082affffffff02c0d8a7000000000017a914d129842dbe1ee73e69d14d54a8a62784877fb83e87108428030000000017a914e483fe5491d8ef5acf043fac5eb1af0f049a80318702473044022035c13c5fdf5f5d07c2101176db8a9c727cec9c31c612b15ae0a4cbdeb25b4dc2022046849e039477aa67fb60e24635668ae1de0bddb9ade3eac2d5ca350898d43c2b01210344715d54ec59240a4ae9f5d8e469f3933a7b03d5c09e15ac3ff53239ea1041b800000000").unwrap());
        Ok(())
    }
}
