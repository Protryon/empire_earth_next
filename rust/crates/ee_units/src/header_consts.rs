//! TEST ONLY: every constant of this crate that copies one of the original's (an action, goal, category, movement
//! domain, unit type, family, resource, tech or trigger) is checked against the C++ headers (include/ee/*.h) by
//! name. Two constants written from memory were wrong once (round 12's heal goal kind 13 for `kGoalHealUnit` 15,
//! round 13's `kMovementSea | kMovementSpace` written 2 | 4): this test reads every `const` of `src/*.rs`, maps its
//! name to the header's (`ACTION_GET_NEXT_MOVE_WAYPOINT` -> `kActionGetNextMoveWaypoint`; the exceptions below),
//! evaluates both sides and fails on a value that differs or a name the headers do not define.

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use std::path::{Path, PathBuf};

    /// the Rust prefixes of copied constants and the headers' (longest first)
    const PREFIXES: &[(&str, &str)] = &[
        ("EE_UNIT_CATEGORY_", "kUnitCategory"),
        ("EE_UNIT_TYPE_", "kUnitType"),
        ("EE_MOVEMENT_", "kMovement"),
        ("EE_ACTION_", "kAction"),
        ("EE_TECH_", "kTech"),
        ("CATEGORY_", "kUnitCategory"),
        ("UNIT_TYPE_", "kUnitType"),
        ("MOVEMENT_", "kMovement"),
        ("ACTION_", "kAction"),
        ("TRIGGER_", "kTrigger"),
        ("FAMILY_", "kUnitFamily"),
        ("RESOURCE_", "kResource"),
        ("GOAL_", "kGoal"),
    ];

    /// names that do not follow the rule: the Rust name and the header's
    const RENAMES: &[(&str, &str)] = &[
        ("UNIT_TYPE_RADIO_MAN", "kUnitTypeInf10RadioMan"),
        ("MOVEMENT_SEA_OR_SPACE", "kMovementSeaOrSpace"),
    ];

    /// copies with no header constant, and why
    const NO_HEADER: &[&str] = &[
        // no goal: the hosts' -1 for a NULL mGoalState
        "GOAL_NONE",
    ];

    fn camel(rest: &str) -> String {
        rest.split('_')
            .map(|w| {
                let mut c = w.chars();
                match c.next() {
                    Some(f) => f.to_ascii_uppercase().to_string() + &c.as_str().to_ascii_lowercase(),
                    None => String::new(),
                }
            })
            .collect()
    }

    fn header_name(name: &str) -> Option<String> {
        if let Some((_, h)) = RENAMES.iter().find(|(r, _)| *r == name) {
            return Some((*h).to_string());
        }
        PREFIXES.iter().find(|(p, _)| name.starts_with(p)).map(|(p, h)| format!("{h}{}", camel(&name[p.len()..])))
    }

    /// an integer expression: literals (decimal, hex, `_`), names, `|`, `+`, unary `-`, `<<`, parentheses
    fn eval(expr: &str, names: &dyn Fn(&str) -> Option<i64>) -> Option<i64> {
        let e = expr.trim();
        let e = e.trim_end_matches(|c: char| c == 'L' || c == 'u' || c == 'U');
        if e.starts_with('(') && e.ends_with(')') {
            // only when the outer parentheses match each other
            let inner = &e[1..e.len() - 1];
            let mut depth = 0i32;
            let mut whole = true;
            for ch in inner.chars() {
                depth += match ch {
                    '(' => 1,
                    ')' => -1,
                    _ => 0,
                };
                if depth < 0 {
                    whole = false;
                }
            }
            if whole && depth == 0 {
                return eval(inner, names);
            }
        }
        for op in ["|", "+", "<<"] {
            // the last top-level operator (left to right associativity)
            let mut depth = 0i32;
            let bytes = e.as_bytes();
            let mut at = None;
            let mut i = 0;
            while i < bytes.len() {
                match bytes[i] {
                    b'(' => depth += 1,
                    b')' => depth -= 1,
                    _ => {
                        if depth == 0 && i > 0 && e[i..].starts_with(op) {
                            at = Some(i);
                        }
                    }
                }
                i += 1;
            }
            if let Some(i) = at {
                let l = eval(&e[..i], names)?;
                let r = eval(&e[i + op.len()..], names)?;
                return Some(match op {
                    "|" => l | r,
                    "+" => l + r,
                    _ => l << r,
                });
            }
        }
        if let Some(rest) = e.strip_prefix('-') {
            return eval(rest, names).map(|v| -v);
        }
        let digits = e.replace('_', "");
        if let Some(hex) = digits.strip_prefix("0x").or_else(|| digits.strip_prefix("0X")) {
            return i64::from_str_radix(hex, 16).ok();
        }
        if let Ok(v) = digits.parse::<i64>() {
            return Some(v);
        }
        names(e)
    }

    fn root() -> PathBuf {
        Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..")
    }

    /// every `kName = expr` (enumerators and `const` definitions) of include/ee/*.h, unevaluated
    fn header_definitions() -> BTreeMap<String, Vec<String>> {
        let mut defs: BTreeMap<String, Vec<String>> = BTreeMap::new();
        let dir = root().join("include/ee");
        let mut files: Vec<_> = std::fs::read_dir(&dir).expect("include/ee").flatten().map(|e| e.path()).collect();
        files.sort();
        for path in files {
            if path.extension().and_then(|x| x.to_str()) != Some("h") {
                continue;
            }
            let Ok(text) = std::fs::read(&path) else { continue };
            let text = String::from_utf8_lossy(&text);
            for line in text.lines() {
                let code = line.split("//").next().unwrap_or("");
                let mut rest = code;
                while let Some(at) = rest.find('k') {
                    let tail = &rest[at..];
                    let before_ok = at == 0 || !rest.as_bytes()[at - 1].is_ascii_alphanumeric() && rest.as_bytes()[at - 1] != b'_';
                    let name_len = tail.bytes().take_while(|b| b.is_ascii_alphanumeric() || *b == b'_').count();
                    let name = &tail[..name_len];
                    let after = tail[name_len..].trim_start();
                    if before_ok
                        && name.len() > 1
                        && name.as_bytes()[1].is_ascii_uppercase()
                        && after.starts_with('=')
                        && !after.starts_with("==")
                    {
                        let value: String = after[1..].chars().take_while(|c| *c != ',' && *c != ';' && *c != '}').collect();
                        defs.entry(name.to_string()).or_default().push(value.trim().to_string());
                    }
                    rest = &tail[name_len.max(1)..];
                }
            }
        }
        defs
    }

    fn header_value(defs: &BTreeMap<String, Vec<String>>, name: &str, depth: u32) -> Vec<i64> {
        if depth > 8 {
            return Vec::new();
        }
        let Some(exprs) = defs.get(name) else { return Vec::new() };
        exprs
            .iter()
            .filter_map(|e| eval(e, &|n| header_value(defs, n, depth + 1).first().copied()))
            .collect()
    }

    /// every `const NAME: T = expr;` of this crate's sources: (file, name, the Rust expression)
    fn rust_constants() -> Vec<(String, String, String)> {
        let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("src");
        let mut out = Vec::new();
        let mut files: Vec<_> = std::fs::read_dir(&dir).expect("src").flatten().map(|e| e.path()).collect();
        files.sort();
        for path in files {
            let text = std::fs::read_to_string(&path).expect("source");
            let file = path.file_name().unwrap().to_string_lossy().to_string();
            for line in text.lines() {
                let l = line.trim_start();
                let l = l.strip_prefix("pub(crate) ").or_else(|| l.strip_prefix("pub ")).unwrap_or(l);
                let Some(l) = l.strip_prefix("const ") else { continue };
                let Some((name, rest)) = l.split_once(':') else { continue };
                let Some((_, value)) = rest.split_once('=') else { continue };
                let Some(value) = value.split(';').next() else { continue };
                out.push((file.clone(), name.trim().to_string(), value.trim().to_string()));
            }
        }
        out
    }

    #[test]
    fn copied_constants_equal_the_headers() {
        let defs = header_definitions();
        assert!(defs.contains_key("kGoalHealUnit") && defs.contains_key("kActionIdle"), "headers not read");
        let mut checked = 0;
        let mut errors = Vec::new();
        for (file, name, expr) in rust_constants() {
            if NO_HEADER.contains(&name.as_str()) {
                continue;
            }
            let Some(cpp) = header_name(&name) else { continue };
            let Some(rust) = eval(&expr, &|_| None) else {
                errors.push(format!("{file}: {name} = {expr}: not an integer expression"));
                continue;
            };
            let values = header_value(&defs, &cpp, 0);
            if values.is_empty() {
                errors.push(format!("{file}: {name}: no {cpp} in include/ee"));
            } else if !values.contains(&rust) {
                errors.push(format!("{file}: {name} = {rust}, but {cpp} = {values:?}"));
            }
            checked += 1;
        }
        assert!(errors.is_empty(), "{} constants differ from the headers:\n{}", errors.len(), errors.join("\n"));
        assert!(checked > 100, "only {checked} constants checked");
    }

    #[test]
    fn the_checker_catches_the_known_slips() {
        let defs = header_definitions();
        assert_eq!(header_value(&defs, "kGoalHealUnit", 0), vec![15]);
        assert_eq!(header_value(&defs, "kMovementSeaOrSpace", 0), vec![12]);
        assert_eq!(header_name("GOAL_HEAL_UNIT").as_deref(), Some("kGoalHealUnit"));
        assert_eq!(header_name("EE_ACTION_MOVE_TO_STORAGE").as_deref(), Some("kActionMoveToStorage"));
        assert_eq!(eval("4 | 8", &|_| None), Some(12));
        assert_eq!(eval("0xffff_ffff", &|_| None), Some(0xffff_ffff));
        assert_eq!(eval("-1", &|_| None), Some(-1));
    }
}
