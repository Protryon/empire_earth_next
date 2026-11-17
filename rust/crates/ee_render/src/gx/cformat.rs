//! C library behaviour the C++ relied on: `atof`, `strtol`, `strtoul` on environment variables and `printf("%g")` in the
//! `EE_GX_STATS` lines.

/// `strtol(text, NULL, 10)`: optional whitespace and sign, then the decimal digits; 0 without any.
pub fn strtol_10(text: &str) -> i64 {
    let bytes = text.trim_start().as_bytes();
    let (negative, digits) = match bytes.first() {
        Some(b'-') => (true, &bytes[1..]),
        Some(b'+') => (false, &bytes[1..]),
        _ => (false, bytes),
    };
    let mut value: i64 = 0;
    for &c in digits {
        if !c.is_ascii_digit() {
            break;
        }
        value = value.wrapping_mul(10).wrapping_add(i64::from(c - b'0'));
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

/// `strtoul(text, NULL, 0)`: decimal, `0x` hexadecimal or leading-zero octal.
pub fn strtoul_0(text: &str) -> u32 {
    let text = text.trim_start();
    let (radix, digits) = if let Some(hex) = text.strip_prefix("0x").or(text.strip_prefix("0X")) {
        (16, hex)
    } else if text.len() > 1 && text.starts_with('0') {
        (8, &text[1..])
    } else {
        (10, text)
    };
    let mut value: u32 = 0;
    for c in digits.chars() {
        match c.to_digit(radix) {
            Some(d) => value = value.wrapping_mul(radix).wrapping_add(d),
            None => break,
        }
    }
    value
}

/// `atof`: the longest number at the start of `text` (`strtod`'s decimal forms), 0 without one.
pub fn atof(text: &str) -> f64 {
    let text = text.trim_start();
    let bytes = text.as_bytes();
    let mut end = 0;
    if matches!(bytes.first(), Some(b'+' | b'-')) {
        end += 1;
    }
    let digits_start = end;
    while end < bytes.len() && bytes[end].is_ascii_digit() {
        end += 1;
    }
    if end < bytes.len() && bytes[end] == b'.' {
        end += 1;
        while end < bytes.len() && bytes[end].is_ascii_digit() {
            end += 1;
        }
    }
    if end == digits_start || (end == digits_start + 1 && bytes[digits_start] == b'.') {
        return 0.0;
    }
    if end < bytes.len() && (bytes[end] == b'e' || bytes[end] == b'E') {
        let mut e = end + 1;
        if e < bytes.len() && (bytes[e] == b'+' || bytes[e] == b'-') {
            e += 1;
        }
        let exponent_digits = e;
        while e < bytes.len() && bytes[e].is_ascii_digit() {
            e += 1;
        }
        if e > exponent_digits {
            end = e;
        }
    }
    text[..end].parse().unwrap_or(0.0)
}

/// `printf("%g", value)`: six significant digits, trailing zeros removed, scientific below 1e-4 and from 1e6.
pub fn format_g(value: f64) -> String {
    if value.is_nan() {
        return if value.is_sign_negative() { "-nan" } else { "nan" }.to_owned();
    }
    if value.is_infinite() {
        return if value < 0.0 { "-inf" } else { "inf" }.to_owned();
    }
    if value == 0.0 {
        return if value.is_sign_negative() { "-0" } else { "0" }.to_owned();
    }
    // %g decides by the exponent of the value rounded to six significant digits
    let scientific = format!("{value:.5e}");
    let (mantissa, exponent) = scientific.rsplit_once('e').unwrap();
    let exponent: i32 = exponent.parse().unwrap();
    if !(-4..6).contains(&exponent) {
        let mantissa = mantissa.trim_end_matches('0').trim_end_matches('.');
        let sign = if exponent < 0 { '-' } else { '+' };
        format!("{mantissa}e{sign}{:02}", exponent.abs())
    } else {
        let decimals = (5 - exponent).max(0) as usize;
        let fixed = format!("{value:.decimals$}");
        if fixed.contains('.') {
            fixed.trim_end_matches('0').trim_end_matches('.').to_owned()
        } else {
            fixed
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strtol_reads_a_prefix() {
        assert_eq!(strtol_10("120"), 120);
        assert_eq!(strtol_10("  -5x"), -5);
        assert_eq!(strtol_10("+7 "), 7);
        assert_eq!(strtol_10("abc"), 0);
        assert_eq!(strtol_10(""), 0);
    }

    #[test]
    fn strtoul_detects_the_base() {
        assert_eq!(strtoul_0("3"), 3);
        assert_eq!(strtoul_0("0x10"), 16);
        assert_eq!(strtoul_0("010"), 8);
        assert_eq!(strtoul_0("0"), 0);
        assert_eq!(strtoul_0("xyz"), 0);
    }

    #[test]
    fn atof_reads_a_prefix() {
        assert_eq!(atof("1.5"), 1.5);
        assert_eq!(atof("2"), 2.0);
        assert_eq!(atof(" 3.25x"), 3.25);
        assert_eq!(atof("1e1"), 10.0);
        assert_eq!(atof("1ex"), 1.0);
        assert_eq!(atof(".5"), 0.5);
        assert_eq!(atof("."), 0.0);
        assert_eq!(atof("abc"), 0.0);
        assert_eq!(atof(""), 0.0);
    }

    #[test]
    fn format_g_matches_printf() {
        // golden values from the C library's printf (Python's '%g')
        let golden = [
            (0.0, "0"),
            (1.0, "1"),
            (-1.5, "-1.5"),
            (100000.0, "100000"),
            (999999.0, "999999"),
            (1000000.0, "1e+06"),
            (1234567.0, "1.23457e+06"),
            (0.0001, "0.0001"),
            (0.00001, "1e-05"),
            (1.0 / 3.0, "0.333333"),
            (3e38, "3e+38"),
            (-3e38, "-3e+38"),
            (123.456, "123.456"),
            (0.1, "0.1"),
            (2.5e-7, "2.5e-07"),
        ];
        for (value, expected) in golden {
            assert_eq!(format_g(value), expected, "{value}");
        }
    }

    #[test]
    fn format_g_rounding_can_carry_into_scientific() {
        assert_eq!(format_g(999999.5), "1e+06");
    }
}
