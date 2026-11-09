//! A DirectDraw pixel format as channel masks, and the conversions between one pixel and B, G, R, A bytes.

use crate::abi::{GxPixelFormat, GXPF_ALPHAPIXELS, GXPF_ZBUFFER};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Format {
    pub bits: u32,
    pub mask: [u32; 4],
    pub shift: [u32; 4],
    pub width: [u32; 4],
    pub is_z: bool,
}

impl Format {
    pub fn set(format: &GxPixelFormat) -> Format {
        let flags = format.dwFlags as u32;
        let alpha = if flags & GXPF_ALPHAPIXELS != 0 { format.dwRGBAlphaBitMask as u32 } else { 0 };
        let mask = [format.dwRBitMask as u32, format.dwGBitMask as u32, format.dwBBitMask as u32, alpha];
        let mut f = Format {
            bits: format.dwRGBBitCount as u32,
            mask,
            shift: [0; 4],
            width: [0; 4],
            is_z: flags & GXPF_ZBUFFER != 0,
        };
        for (i, channel) in mask.iter().enumerate() {
            f.shift[i] = if *channel == 0 { 0 } else { channel.trailing_zeros() };
            f.width[i] = channel.count_ones();
        }
        f
    }

    pub fn is_bgra32(&self) -> bool {
        self.bits == 32 && self.mask[0] == 0x00ff_0000 && self.mask[1] == 0x0000_ff00 && self.mask[2] == 0x0000_00ff
    }

    /// A pixel as B, G, R, A bytes (the GPU textures are BGRA8Unorm); no alpha mask reads as 255.
    pub fn to_bgra(&self, pixel: u32) -> [u8; 4] {
        const ORDER: [usize; 4] = [2, 1, 0, 3]; // R, G, B, A -> byte positions in BGRA
        let mut out = [0u8; 4];
        for i in 0..4 {
            let width = self.width[i];
            let value = if width == 0 {
                if i == 3 {
                    255
                } else {
                    0
                }
            } else {
                let mut value = (pixel & self.mask[i]) >> self.shift[i];
                if width >= 8 {
                    value >>= width - 8;
                } else {
                    let max = (1u32 << width) - 1;
                    value = (value * 255 + max / 2) / max;
                }
                value
            };
            out[ORDER[i]] = value as u8;
        }
        out
    }

    pub fn from_bgra(&self, bgra: &[u8]) -> u32 {
        const ORDER: [usize; 4] = [2, 1, 0, 3];
        let mut pixel = 0u32;
        for i in 0..4 {
            let width = self.width[i];
            if width == 0 {
                continue;
            }
            let mut value = u32::from(bgra[ORDER[i]]);
            if width >= 8 {
                value <<= width - 8;
            } else {
                let max = (1u32 << width) - 1;
                value = (value * max + 127) / 255;
            }
            pixel |= (value << self.shift[i]) & self.mask[i];
        }
        pixel
    }

    /// One pixel from `bits / 8` little-endian bytes of a surface's memory.
    pub fn read(&self, bytes: &[u8]) -> u32 {
        if self.bits == 32 {
            u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])
        } else {
            u32::from(u16::from_le_bytes([bytes[0], bytes[1]]))
        }
    }

    pub fn write(&self, bytes: &mut [u8], pixel: u32) {
        if self.bits == 32 {
            bytes[..4].copy_from_slice(&pixel.to_le_bytes());
        } else {
            bytes[..2].copy_from_slice(&(pixel as u16).to_le_bytes());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn golden_conversions_from_the_cpp() {
        // golden_format.txt: GXFormat::Set, ToBGRA and FromBGRA of the C++ this module replaced
        let mut count = 0;
        for (index, line) in include_str!("golden_format.txt").lines().enumerate() {
            let fields: Vec<&str> = line.split_whitespace().collect();
            if fields.is_empty() || fields[0] == "#" {
                continue;
            }
            let hex = |i: usize| u32::from_str_radix(fields[i], 16).unwrap();
            let format = GxPixelFormat::with_flags(hex(0), fields[1].parse().unwrap(), hex(2), hex(3), hex(4), hex(5));
            let f = Format::set(&format);
            let pixel = hex(6);
            let bgra: Vec<u8> = (7..11).map(|i| fields[i].parse::<u8>().unwrap()).collect();
            assert_eq!(f.to_bgra(pixel), bgra.as_slice(), "line {index} to_bgra");
            let back: Vec<u8> = (11..15).map(|i| fields[i].parse::<u8>().unwrap()).collect();
            assert_eq!(f.from_bgra(&back), hex(15), "line {index} from_bgra");
            count += 1;
        }
        assert!(count > 40);
    }

    #[test]
    fn set_reads_the_alpha_mask_only_with_the_flag() {
        let no_alpha = GxPixelFormat::with_flags(crate::abi::GXPF_RGB, 32, 0xff0000, 0xff00, 0xff, 0xff000000);
        assert_eq!(Format::set(&no_alpha).mask[3], 0);
        let z = GxPixelFormat::with_flags(GXPF_ZBUFFER, 16, 0, 0, 0, 0);
        assert!(Format::set(&z).is_z);
    }

    #[test]
    fn bgra32_is_detected_by_masks() {
        assert!(Format::set(&GxPixelFormat::rgb(32, 0xff0000, 0xff00, 0xff)).is_bgra32());
        assert!(!Format::set(&GxPixelFormat::rgb(32, 0xff, 0xff00, 0xff0000)).is_bgra32());
        assert!(!Format::set(&GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f)).is_bgra32());
    }

    #[test]
    fn memory_round_trips_by_width() {
        let f = Format::set(&GxPixelFormat::rgb(32, 0xff0000, 0xff00, 0xff));
        let mut bytes = [0u8; 4];
        f.write(&mut bytes, 0x1234_5678);
        assert_eq!(f.read(&bytes), 0x1234_5678);
        let f = Format::set(&GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f));
        let mut bytes = [0u8; 2];
        f.write(&mut bytes, 0x1_2345);
        assert_eq!(f.read(&bytes), 0x2345);
    }

    #[test]
    fn a_format_without_masks_is_black_and_opaque() {
        assert_eq!(Format::default().to_bgra(0xffff_ffff), [0, 0, 0, 255]);
        assert_eq!(Format::default().from_bgra(&[1, 2, 3, 4]), 0);
    }
}
