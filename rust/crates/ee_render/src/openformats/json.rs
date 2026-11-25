//! The JSON values of a model outside the glTF schema, read by `serde_json`: the EE_model extension (already a value
//! in the parsed document) and a material's extras (their text). `JsonNumber` is how GEOpenFormats.cpp's `JParser`
//! took a number: an index only from an integer token, a float from any number.
//!
//! serde_json is built with `float_roundtrip` (Cargo.toml): a number is the nearest double of its decimal text, as
//! the C++ `from_chars` and the Python converter read it. A repeated key keeps its last value, as in Python's json.

pub(crate) use serde_json::Value;

/// Parses a whole text as one JSON value; `None` when it is not JSON.
pub(crate) fn parse(text: &[u8]) -> Option<Value> {
    serde_json::from_slice(text).ok()
}

pub(crate) trait JsonNumber {
    /// The number as a double (0 for what is not a number).
    fn number(&self) -> f64;
    /// A JSON integer in 0..limit (not a bool, not a float, not above 2^63-1), its value.
    fn index(&self, limit: u64) -> Option<u64>;
}

impl JsonNumber for Value {
    fn number(&self) -> f64 {
        self.as_f64().unwrap_or(0.0)
    }

    fn index(&self, limit: u64) -> Option<u64> {
        self.as_u64().filter(|&v| v <= i64::MAX as u64 && v < limit)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values() {
        let v = parse(br#" {"a":[1,2.5,"x\n",true,false,null],"a":3} "#).unwrap();
        assert_eq!(v.get("a").unwrap().index(10), Some(3)); // the last one wins
    }

    #[test]
    fn integers_are_indexed_only_if_in_range_and_whole() {
        assert_eq!(parse(b"5").unwrap().index(6), Some(5));
        assert_eq!(parse(b"5").unwrap().index(5), None);
        assert_eq!(parse(b"5.0").unwrap().index(6), None);
        assert_eq!(parse(b"-1").unwrap().index(6), None);
        assert_eq!(parse(b"true").unwrap().index(6), None);
        assert_eq!(parse(b"9223372036854775808").unwrap().index(u64::MAX), None);
        assert_eq!(parse(b"9223372036854775807").unwrap().index(u64::MAX), Some(i64::MAX as u64));
    }

    #[test]
    fn refusals_are_strict() {
        for bad in [
            &b"[1,]"[..],
            b"{\"a\":1,}",
            b"01x",
            b"1.",
            b".5",
            b"-",
            b"tru",
            b"\"a",
            b"\"\t\"",
            b"\"\\q\"",
            b"[1 2]",
            b"{\"a\" 1}",
            b"1 2",
        ] {
            assert_eq!(parse(bad), None, "{bad:?}");
        }
    }

    #[test]
    fn numbers_are_the_nearest_double() {
        assert_eq!(parse(b"1e-400").unwrap().number(), 0.0);
        assert_eq!(parse(b"4.684328e-39").unwrap().number(), 4.684328e-39); // subnormal in f32, fine in f64
        assert_eq!(parse(b"0.1").unwrap().number(), 0.1);
        assert_eq!(parse(b"7").unwrap().number(), 7.0);
        assert_eq!(parse(b"\"7\"").unwrap().number(), 0.0);
        // texts whose nearest double a parser without `float_roundtrip` may miss by one unit in the last place
        for text in ["0.000001192092896", "8.988465674311579e307", "2.2250738585072011e-308", "1.7976931348623157e308"]
        {
            assert_eq!(parse(text.as_bytes()).unwrap().number(), text.parse::<f64>().unwrap(), "{text}");
        }
    }
}
