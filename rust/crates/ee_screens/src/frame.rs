//! The game's framed form background (`EEUIBGenericBackground`: the message box's, and every database background of
//! the generic kind): its tinted panel inside the side ornaments, then the frame pieces its flags ask for (1 the
//! edges and the small corners, 2 the large corners instead of the small ones, 4 the side ornaments, which inset the
//! rest), each clipped to the redraw rectangle and mirrored into its corner or turned onto its edge. Over a host
//! ([`FrameHost`]) that draws the panel and each piece (`User Interface Thread/EEUIBGenericBackground.cpp`: the
//! bitmap's stretch, its texture coordinates' order, its location and the rasterizer's draw).

use core::ffi::c_long;

use ee_ge::types::GeRectangle;

/// The frame texture's cells: the edges, the large corners, the small corners, the side ornaments.
pub const EE_SCREENS_FRAME_CELL_EDGE: u32 = 0;
pub const EE_SCREENS_FRAME_CELL_LARGE_CORNER: u32 = 1;
pub const EE_SCREENS_FRAME_CELL_CORNER: u32 = 2;
pub const EE_SCREENS_FRAME_CELL_SIDE: u32 = 3;

/// No reorder of the texture coordinates.
pub const EE_SCREENS_FRAME_KEEP: u32 = 0x100;

/// An order of the texture coordinates: the new top-left, top-right, bottom-right and bottom-left as indices into the
/// current ones (0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left), two bits each from the low bits.
const fn order(top_left: u32, top_right: u32, bottom_right: u32, bottom_left: u32) -> u32 {
    top_left | top_right << 2 | bottom_right << 4 | bottom_left << 6
}
/// A quarter turn (the top and bottom edges).
const FRAME_ROTATE: u32 = order(1, 3, 0, 2);
/// Left and right swapped.
const FRAME_FLIP_H: u32 = order(1, 0, 3, 2);
/// Top and bottom swapped.
const FRAME_FLIP_V: u32 = order(2, 3, 0, 1);
/// Both (a half turn).
const FRAME_FLIP_BOTH: u32 = order(3, 2, 1, 0);

/// One piece: the bitmap stretched from `texture` (the piece's whole rectangle, in the frame's pixels) onto
/// `destination` with the cell's texture, its texture coordinates reordered by each of `orders` in turn
/// (`EE_SCREENS_FRAME_KEEP` for none; the scales swapped after the first when `swap_scale`), placed at `location`.
/// Rectangles are left, top, right, bottom.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensFramePiece {
    pub texture: [f32; 4],
    pub destination: [f32; 4],
    pub location: [f32; 2],
    pub cell: u32,
    pub orders: [u32; 3],
    pub swap_scale: bool,
}

/// The background's draws.
pub trait FrameHost {
    /// The tinted panel over `inner` (left, top, right, bottom); the rasterizer's result.
    fn fill(&mut self, inner: [f32; 4]) -> c_long;
    /// One frame piece.
    fn piece(&mut self, piece: &EeScreensFramePiece);
}

/// The background's rectangle, scale and flags.
#[derive(Clone, Copy, Debug)]
pub struct Frame {
    pub rectangle: GeRectangle,
    pub scale_x: f32,
    pub scale_y: f32,
    pub flags: u32,
}

fn sides(r: &GeRectangle) -> [f32; 4] {
    [r.left, r.top, r.right, r.bottom]
}

/// `DrawCorner`: the cell over `corner` mirrored into the corner `rotation` names (0 top-left, 1 bottom-left, 2
/// bottom-right, 3 top-right), `clipped` its visible part.
fn corner<H: FrameHost + ?Sized>(h: &mut H, cell: u32, clipped: &GeRectangle, corner: &GeRectangle, rotation: u32) {
    let c = corner;
    let d = clipped;
    let mut destination = *d;
    let mut first = EE_SCREENS_FRAME_KEEP;
    if rotation == 1 || rotation == 2 {
        destination.top = c.bottom - d.bottom + c.top;
        destination.bottom = c.bottom - d.top + c.top;
    }
    if rotation == 2 || rotation == 3 {
        destination.left = c.right - d.right + c.left;
        destination.right = c.right - d.left + c.left;
    }
    match rotation {
        1 => first = FRAME_FLIP_V,
        2 => first = FRAME_FLIP_BOTH,
        3 => first = FRAME_FLIP_H,
        _ => {}
    }
    h.piece(&EeScreensFramePiece {
        texture: sides(c),
        destination: sides(&destination),
        location: [d.left, d.top],
        cell,
        orders: [first, EE_SCREENS_FRAME_KEEP, EE_SCREENS_FRAME_KEEP],
        swap_scale: false,
    });
}

/// `DrawEdge`: the edge cell over `edge`, turned a quarter (`rotate`: the top and bottom edges) and flipped as asked,
/// `clipped` its visible part.
fn edge<H: FrameHost + ?Sized>(
    h: &mut H,
    clipped: &GeRectangle,
    edge: &GeRectangle,
    rotate: bool,
    flip_v: bool,
    flip_h: bool,
) {
    let (d, t) = (clipped, edge);
    let mut piece = EeScreensFramePiece {
        texture: sides(t),
        destination: sides(d),
        location: [d.left, d.top],
        cell: EE_SCREENS_FRAME_CELL_EDGE,
        orders: [EE_SCREENS_FRAME_KEEP; 3],
        swap_scale: false,
    };
    if rotate {
        // the destination turned into the texture's frame, the texture's sides swapped
        piece.destination = [t.bottom - d.bottom + t.top, d.left, t.bottom - d.top + t.top, d.right];
        piece.texture = [t.top, t.left, t.bottom, t.right];
        piece.orders[0] = FRAME_ROTATE;
        piece.swap_scale = true;
    }
    if flip_h {
        piece.orders[1] = FRAME_FLIP_H;
    }
    if flip_v {
        piece.orders[2] = FRAME_FLIP_V;
    }
    h.piece(&piece);
}

impl Frame {
    /// The clipped part of a piece, `None` when it is out of the redraw rectangle.
    fn clip(piece: &GeRectangle, clip: &GeRectangle) -> Option<GeRectangle> {
        piece.and(clip)
    }

    /// `DrawCorners`: the four small corners (inside the side ornaments).
    fn small_corners<H: FrameHost + ?Sized>(&self, h: &mut H, clip: &GeRectangle) {
        let (r, sx, sy) = (&self.rectangle, self.scale_x, self.scale_y);
        let inset = if self.flags & 4 != 0 { sx * 17.0f32 } else { 0.0f32 };
        let mut c = GeRectangle::default();
        c.left = r.left + inset;
        c.right = sx * 7.0f32 - 1.0f32 + c.left;
        c.top = r.top;
        c.bottom = sy * 7.0f32 - 1.0f32 + c.top;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_CORNER, &c, clip, 0);
        c.top = r.bottom - sy * 7.0f32 + 1.0f32;
        c.bottom = r.bottom;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_CORNER, &c, clip, 1);
        c.left = r.right - sx * 7.0f32 - inset + 1.0f32;
        c.right = r.right - inset;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_CORNER, &c, clip, 2);
        c.top = r.top;
        c.bottom = sy * 7.0f32 - 1.0f32 + r.top;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_CORNER, &c, clip, 3);
    }

    fn corner_at<H: FrameHost + ?Sized>(
        &self,
        h: &mut H,
        cell: u32,
        c: &GeRectangle,
        clip: &GeRectangle,
        rotation: u32,
    ) {
        if let Some(clipped) = Self::clip(c, clip) {
            corner(h, cell, &clipped, c, rotation);
        }
    }

    fn edge_at<H: FrameHost + ?Sized>(
        &self,
        h: &mut H,
        e: &GeRectangle,
        clip: &GeRectangle,
        flags: (bool, bool, bool),
    ) {
        if let Some(clipped) = Self::clip(e, clip) {
            edge(h, &clipped, e, flags.0, flags.1, flags.2);
        }
    }

    /// `DrawEdges`: the left and right edges, then the top and bottom ones (turned; the bottom one flipped).
    fn edges<H: FrameHost + ?Sized>(&self, h: &mut H, clip: &GeRectangle) {
        let (r, sx, sy) = (&self.rectangle, self.scale_x, self.scale_y);
        let inset = if self.flags & 4 != 0 { sx * 18.0f32 - 1.0f32 } else { 0.0f32 };
        let mut e = GeRectangle::default();
        e.left = r.left + inset;
        e.right = sx * 7.0f32 - 1.0f32 + e.left;
        e.top = r.top + sy * 7.0f32;
        e.bottom = r.bottom - sy * 7.0f32;
        self.edge_at(h, &e, clip, (false, false, false));
        e.left = r.right - (sx * 7.0f32 - 1.0f32) - inset;
        e.right = r.right - inset;
        self.edge_at(h, &e, clip, (false, false, true));
        e.left = r.left + sx * 7.0f32 + inset;
        e.right = r.right - inset - sx * 7.0f32;
        e.top = r.top;
        e.bottom = sy * 7.0f32 - 1.0f32 + e.top;
        self.edge_at(h, &e, clip, (true, false, false));
        e.top = r.bottom - (sy * 7.0f32 - 1.0f32);
        e.bottom = r.bottom;
        self.edge_at(h, &e, clip, (true, true, false));
    }

    /// `DrawFrameCorners`: the four large corners.
    fn large_corners<H: FrameHost + ?Sized>(&self, h: &mut H, clip: &GeRectangle) {
        let (r, sx, sy) = (&self.rectangle, self.scale_x, self.scale_y);
        let inset = if self.flags & 4 != 0 { sx * 18.0f32 - 1.0f32 } else { 0.0f32 };
        let mut c = GeRectangle::default();
        c.left = r.left + inset;
        c.right = sx * 28.0f32 - 1.0f32 + c.left;
        c.top = r.top;
        c.bottom = sy * 38.0f32 - 1.0f32 + c.top;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_LARGE_CORNER, &c, clip, 0);
        c.top = r.bottom - sy * 38.0f32 + 1.0f32;
        c.bottom = r.bottom;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_LARGE_CORNER, &c, clip, 1);
        c.left = r.right - sx * 28.0f32 + 1.0f32 - inset;
        c.right = r.right - inset;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_LARGE_CORNER, &c, clip, 2);
        c.top = r.top;
        c.bottom = sy * 38.0f32 - 1.0f32 + r.top;
        self.corner_at(h, EE_SCREENS_FRAME_CELL_LARGE_CORNER, &c, clip, 3);
    }

    /// `DrawSides`: the two side ornaments at mid height.
    fn side_ornaments<H: FrameHost + ?Sized>(&self, h: &mut H, clip: &GeRectangle) {
        let (r, sx, sy) = (&self.rectangle, self.scale_x, self.scale_y);
        let mut s = GeRectangle::default();
        s.left = r.left;
        s.right = sx * 18.0f32 - 1.0f32 + s.left;
        s.top = (r.bottom - r.top) * 0.5f32 + r.top - sy * 10.0f32;
        s.bottom = sy * 20.0f32 - 1.0f32 + s.top;
        for pass in 0..2 {
            if pass == 1 {
                s.left = r.right - sx * 18.0f32 + 1.0f32;
                s.right = r.right;
            }
            if let Some(clipped) = Self::clip(&s, clip) {
                h.piece(&EeScreensFramePiece {
                    texture: sides(&s),
                    destination: sides(&clipped),
                    location: [clipped.left, clipped.top],
                    cell: EE_SCREENS_FRAME_CELL_SIDE,
                    orders: [EE_SCREENS_FRAME_KEEP; 3],
                    swap_scale: false,
                });
            }
        }
    }

    /// `DrawBackground`: the panel inside the side ornaments' inset, clipped to `clip` and a pixel larger right and
    /// down, then the frame pieces the flags ask for, clipped to the background's visible part. The panel's result.
    pub fn draw<H: FrameHost + ?Sized>(&self, h: &mut H, clip: &GeRectangle) -> c_long {
        let inset = if self.flags & 4 != 0 { 18.0f32 } else { 0.0f32 };
        let mut visible = self.rectangle;
        visible.and_in_place(clip);
        let mut inner = self.rectangle;
        inner.left += inset;
        inner.right -= inset;
        inner.and_in_place(clip);
        inner.right += 1.0f32;
        inner.bottom += 1.0f32;
        let result = h.fill(sides(&inner));
        if self.flags & 1 != 0 {
            self.edges(h, &visible);
            if self.flags & 2 == 0 {
                self.small_corners(h, &visible);
            }
        }
        if self.flags & 2 != 0 {
            self.large_corners(h, &visible);
        }
        if self.flags & 4 != 0 {
            self.side_ornaments(h, &visible);
        }
        result
    }
}

#[cfg(test)]
mod golden;
