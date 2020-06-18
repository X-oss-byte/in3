//! Types common to all modules.
use std::fmt;

use rustc_hex::{FromHex, ToHex};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use serde::de::{Error, Visitor};
use serde::export::Formatter;

/// Newtype wrapper around vector of bytes
#[derive(Debug, PartialEq, Eq, Default, Hash, Clone)]
pub struct Bytes(pub Vec<u8>);

impl From<Vec<u8>> for Bytes {
    fn from(vec: Vec<u8>) -> Bytes {
        Bytes(vec)
    }
}

impl From<&[u8]> for Bytes {
    fn from(slice: &[u8]) -> Bytes {
        Bytes(slice.to_vec())
    }
}

impl Serialize for Bytes {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: Serializer
    {
        let mut serialized = "0x".to_owned();
        serialized.push_str(self.0.to_hex().as_str());
        serializer.serialize_str(serialized.as_ref())
    }
}

impl<'a> Deserialize<'a> for Bytes {
    fn deserialize<D>(deserializer: D) -> Result<Bytes, D::Error>
        where D: Deserializer<'a> {
        deserializer.deserialize_str(BytesVisitor)
    }
}

struct BytesVisitor;

impl<'a> Visitor<'a> for BytesVisitor {
    type Value = Bytes;

    fn expecting(&self, formatter: &mut Formatter) -> fmt::Result {
        write!(formatter, "a hex string prefixed with '0x'")
    }

    fn visit_str<E>(self, value: &str) -> Result<Self::Value, E> where E: Error {
        if value.starts_with("0x") {
            Ok(FromHex::from_hex(&value[2..]).map_err(|e| Error::custom(format!("Invalid hex: {}", e)))?.into())
        } else {
            Err(Error::custom("invalid string"))
        }
    }
}
