//! The damage tables' storage (R5b.2, third round): `gDamageTable` and its two difficulty twins
//! (`EETerrainTypeTable::LoadDamageTables`), one row per unit family of `DAMAGE_COLUMNS` attack kinds.
//!
//! The rows are boxed and never move until the next load or clear, so the C++ can keep `gDamageTable` as an array of
//! row pointers into this storage (`ee_combat_damage_table_row`); `ee_combat_damage_factor` is the checked accessor.
//! The tables are written only while the database loads, before any reader runs.

use std::sync::RwLock;

use crate::{damage_row, EeCombatDamageRowIn, DAMAGE_COLUMNS};

/// The base table (`gDamageTable`).
pub const EE_COMBAT_DAMAGE_BASE: u32 = 0;
/// The first difficulty twin (`gDamageTableScaled1`).
pub const EE_COMBAT_DAMAGE_SCALED1: u32 = 1;
/// The second difficulty twin (`gDamageTableScaled2`).
pub const EE_COMBAT_DAMAGE_SCALED2: u32 = 2;

type Row = Box<[f32; DAMAGE_COLUMNS]>;

/// The three tables, row by row.
#[derive(Default)]
pub struct DamageTables {
    tables: [Vec<Row>; 3],
}

impl DamageTables {
    /// The tables of `rows` percentage rows (each through `damage_row`).
    pub fn build(rows: &[[i32; DAMAGE_COLUMNS]], scale: f32, twin1: f32, twin2: f32) -> DamageTables {
        let mut out = DamageTables::default();
        for percent in rows {
            let row = damage_row(&EeCombatDamageRowIn { percent: *percent, scale, twin1, twin2 });
            out.tables[0].push(Box::new(row.base));
            out.tables[1].push(Box::new(row.scaled1));
            out.tables[2].push(Box::new(row.scaled2));
        }
        out
    }

    pub fn rows(&self) -> usize {
        self.tables[0].len()
    }

    /// The row of a table, `None` out of range.
    pub fn row(&self, table: u32, row: usize) -> Option<&[f32; DAMAGE_COLUMNS]> {
        self.tables.get(table as usize)?.get(row).map(|r| &**r)
    }

    /// `gDamageTable[row][column]`; 0 for an index out of the table (where the C++ read past its arrays).
    pub fn factor(&self, row: i32, column: i32) -> f32 {
        if row < 0 || column < 0 || column as usize >= DAMAGE_COLUMNS {
            return 0.0;
        }
        self.row(EE_COMBAT_DAMAGE_BASE, row as usize).map_or(0.0, |r| r[column as usize])
    }
}

static TABLES: RwLock<Option<DamageTables>> = RwLock::new(None);

fn with<T>(f: impl FnOnce(&DamageTables) -> T, none: T) -> T {
    let guard = TABLES.read().unwrap_or_else(|e| e.into_inner());
    match guard.as_ref() {
        Some(t) => f(t),
        None => none,
    }
}

/// Loads the tables from `count` rows of percentages (replacing any loaded ones); returns the row count.
///
/// # Safety
/// `percent` must point to `count` rows (or be NULL with `count` 0). Every row pointer handed out before is invalid
/// afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_damage_table_load(
    percent: *const [i32; DAMAGE_COLUMNS],
    count: u32,
    scale: f32,
    twin1: f32,
    twin2: f32,
) -> u32 {
    let rows: &[[i32; DAMAGE_COLUMNS]] = if percent.is_null() || count == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(percent, count as usize) }
    };
    let tables = DamageTables::build(rows, scale, twin1, twin2);
    let n = tables.rows() as u32;
    *TABLES.write().unwrap_or_else(|e| e.into_inner()) = Some(tables);
    n
}

/// Frees the tables (`ClearDamageTables`). Every row pointer handed out before is invalid afterwards.
#[no_mangle]
pub extern "C" fn ee_combat_damage_table_clear() {
    *TABLES.write().unwrap_or_else(|e| e.into_inner()) = None;
}

/// The loaded row count (0 when none).
#[no_mangle]
pub extern "C" fn ee_combat_damage_table_rows() -> u32 {
    with(|t| t.rows() as u32, 0)
}

/// Row `row` of table `table` (`EE_COMBAT_DAMAGE_*`): `DAMAGE_COLUMNS` floats, valid until the next load or clear;
/// NULL out of range.
#[no_mangle]
pub extern "C" fn ee_combat_damage_table_row(table: u32, row: u32) -> *const f32 {
    with(|t| t.row(table, row as usize).map_or(std::ptr::null(), |r| r.as_ptr()), std::ptr::null())
}

/// `gDamageTable[row][column]` (the base table): 0 out of range or before a load.
#[no_mangle]
pub extern "C" fn ee_combat_damage_factor(row: i32, column: i32) -> f32 {
    with(|t| t.factor(row, column), 0.0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rows() -> Vec<[i32; DAMAGE_COLUMNS]> {
        (0..3).map(|r| std::array::from_fn(|c| (r * 100 + c as i32) % 250)).collect()
    }

    #[test]
    fn build_matches_damage_row() {
        let t = DamageTables::build(&rows(), 0.01, 0.75, 1.25);
        assert_eq!(t.rows(), 3);
        for (r, percent) in rows().iter().enumerate() {
            let want = damage_row(&EeCombatDamageRowIn { percent: *percent, scale: 0.01, twin1: 0.75, twin2: 1.25 });
            assert_eq!(t.row(EE_COMBAT_DAMAGE_BASE, r).unwrap(), &want.base);
            assert_eq!(t.row(EE_COMBAT_DAMAGE_SCALED1, r).unwrap(), &want.scaled1);
            assert_eq!(t.row(EE_COMBAT_DAMAGE_SCALED2, r).unwrap(), &want.scaled2);
            for c in 0..DAMAGE_COLUMNS {
                assert_eq!(t.factor(r as i32, c as i32).to_bits(), want.base[c].to_bits());
            }
        }
    }

    #[test]
    fn factor_out_of_range_is_zero() {
        let t = DamageTables::build(&rows(), 0.01, 1.0, 1.0);
        assert_eq!(t.factor(-1, 0), 0.0);
        assert_eq!(t.factor(0, -1), 0.0);
        assert_eq!(t.factor(3, 0), 0.0);
        assert_eq!(t.factor(0, DAMAGE_COLUMNS as i32), 0.0);
        assert!(t.row(3, 0).is_none());
    }

    #[test]
    fn the_c_tables_load_hand_out_stable_rows_and_clear() {
        let rows = rows();
        // SAFETY: three rows
        let n = unsafe { ee_combat_damage_table_load(rows.as_ptr(), rows.len() as u32, 0.01, 0.5, 2.0) };
        assert_eq!(n, 3);
        assert_eq!(ee_combat_damage_table_rows(), 3);
        let p = ee_combat_damage_table_row(EE_COMBAT_DAMAGE_SCALED2, 1);
        assert!(!p.is_null());
        // SAFETY: a live row of DAMAGE_COLUMNS floats
        let v = unsafe { *p.add(5) };
        assert_eq!(v, 2.0 * (rows[1][5] as f32 * 0.01));
        assert_eq!(ee_combat_damage_factor(2, 7), rows[2][7] as f32 * 0.01);
        assert!(ee_combat_damage_table_row(0, 3).is_null());
        ee_combat_damage_table_clear();
        assert_eq!(ee_combat_damage_table_rows(), 0);
        assert_eq!(ee_combat_damage_factor(2, 7), 0.0);
    }
}
