//! Point placement and relaxation inside a ring (`Unit09.cpp`).

use crate::map::{Ring, URandomState};
use crate::math::{lrintf, Point, Rotation, Vector, TWO_PI};

const RING_ANGLE_STEP: f32 = 0.0175;

/// One end of the closest pair: a point that can move, or a fixed one.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum End {
    Moving(usize),
    Fixed(usize),
}

/// `FindClosestPoint`: the squared distance to the nearest point and its index (`None` for an empty list).
pub fn find_closest_point(points: &[Point], point: Point) -> (f32, Option<usize>) {
    let mut best = f32::MAX;
    let mut closest = None;
    for (i, p) in points.iter().enumerate() {
        let distance = point.distance_to_squared(*p);
        if distance < best {
            best = distance;
            closest = Some(i);
        }
    }
    (best, closest)
}

/// `FindClosestPair`, with or without fixed points: the squared distance and the pair (`None` when no pair exists).
pub fn find_closest_pair(points: &[Point], fixed: &[Point]) -> (f32, Option<(usize, End)>) {
    let mut best = f32::MAX;
    let mut pair = None;
    for (i, a) in points.iter().enumerate() {
        for (j, b) in points.iter().enumerate().skip(i + 1) {
            let distance = a.distance_to_squared(*b);
            if distance < best {
                best = distance;
                pair = Some((i, End::Moving(j)));
            }
        }
        for (j, b) in fixed.iter().enumerate() {
            let distance = a.distance_to_squared(*b);
            if distance < best {
                best = distance;
                pair = Some((i, End::Fixed(j)));
            }
        }
    }
    (best, pair)
}

/// `PlacePointOnRing`.
pub fn place_point_on_ring(random: &mut URandomState, ring: &Ring, angle: &mut f32) -> Point {
    let radius = random.generate() * (ring.max_radius - ring.min_radius) + ring.min_radius;
    let offset = Rotation::yaw(*angle).apply(Vector { x: 0.0, y: radius, z: 0.0 });
    let point = Point::new(ring.center.x + offset.x, ring.center.y + offset.y);
    *angle += RING_ANGLE_STEP;
    point
}

/// `PlacePointsOnRing`.
pub fn place_points_on_ring(random: &mut URandomState, ring: &Ring, points: &mut [Point]) {
    for point in points.iter_mut() {
        let distance = random.generate() * (ring.max_radius - ring.min_radius) + ring.min_radius;
        let rotation = Rotation::yaw(random.generate() * TWO_PI);
        let offset = rotation.apply(Vector { x: 0.0, y: distance, z: 0.0 });
        point.x = ring.center.x + offset.x;
        point.y = ring.center.y + offset.y;
    }
}

/// Moves `point` by `direction * step` (`sign` is +1 or -1), then clamps it back between the ring's radii.
fn moved(point: Point, direction: Vector, step: f32, sign: bool, ring: &Ring, minimum: u32, maximum: u32) -> Point {
    let mut next = if sign {
        Point::new(point.x + direction.x * step, point.y + direction.y * step)
    } else {
        Point::new(point.x - direction.x * step, point.y - direction.y * step)
    };
    let radius = if next.distance_to_squared(ring.center) > maximum as f32 {
        Some(ring.max_radius)
    } else if next.distance_to_squared(ring.center) < minimum as f32 {
        Some(ring.min_radius)
    } else {
        None
    };
    if let Some(radius) = radius {
        let mut offset = Vector { x: next.x - ring.center.x, y: next.y - ring.center.y, z: 0.0 };
        offset.normalize();
        offset = offset.scale(radius);
        next = Point::new(ring.center.x + offset.x, ring.center.y + offset.y);
    }
    next
}

/// `SeparateClosestPair`: moves the closest two points a step apart; a fixed point never moves.
pub fn separate_closest_pair(points: &mut [Point], fixed: &[Point], ring: &Ring, step: f32) {
    let minimum = lrintf(ring.min_radius * ring.min_radius) as u32;
    let maximum = lrintf(ring.max_radius * ring.max_radius) as u32;
    let (_, pair) = find_closest_pair(points, fixed);
    // no pair: one point and nothing fixed (the C++ dereferences an unset iterator; only reachable with one player)
    let Some((a, b)) = pair else { return };
    let point_b = match b {
        End::Moving(j) => points[j],
        End::Fixed(j) => fixed[j],
    };
    let mut direction = Vector { x: points[a].x - point_b.x, y: points[a].y - point_b.y, z: 0.0 };
    direction.normalize();
    points[a] = moved(points[a], direction, step, true, ring, minimum, maximum);
    if let End::Moving(j) = b {
        points[j] = moved(points[j], direction, step, false, ring, minimum, maximum);
    }
}

/// `RelaxPoints`: pushes the closest pair apart until its squared distance exceeds a minimum or the iteration limit.
pub fn relax_points(
    points: &mut [Point],
    fixed: &[Point],
    ring: &Ring,
    step: f32,
    minimum: f32,
    iterations: u32,
) -> f32 {
    let mut distance = 0.0f32;
    let mut i = 0u32;
    while distance <= minimum && i < iterations {
        separate_closest_pair(points, fixed, ring, step);
        distance = find_closest_pair(points, fixed).0;
        i += 1;
    }
    distance
}

/// `SpreadPoints`: rounds of pair separation until the average closest squared distance settles within a tolerance.
pub fn spread_points(
    points: &mut [Point],
    fixed: &[Point],
    ring: &Ring,
    step: f32,
    passes: f32,
    tolerance: f32,
    rounds: u32,
) -> f32 {
    let mut average = 0.0f32;
    for _ in 0..rounds {
        let previous = average;
        let mut total = 0.0f32;
        let mut i = 0u32;
        while (i as f32) < passes {
            separate_closest_pair(points, fixed, ring, step);
            total += find_closest_pair(points, fixed).0;
            i += 1;
        }
        average = total / passes;
        let change = average - previous;
        if (if change < 0.0 { -change } else { change }) < tolerance {
            break;
        }
    }
    average
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_random::URandom;

    #[test]
    fn closest_point_is_the_first_minimum() {
        let points = [Point::new(5.0, 0.0), Point::new(1.0, 0.0), Point::new(-1.0, 0.0)];
        assert_eq!(find_closest_point(&points, Point::default()), (1.0, Some(1)));
        assert_eq!(find_closest_point(&[], Point::default()), (f32::MAX, None));
    }

    #[test]
    fn closest_pair_includes_the_fixed_points() {
        let points = [Point::new(0.0, 0.0), Point::new(10.0, 0.0)];
        let fixed = [Point::new(10.0, 1.0)];
        assert_eq!(find_closest_pair(&points, &[]), (100.0, Some((0, End::Moving(1)))));
        assert_eq!(find_closest_pair(&points, &fixed), (1.0, Some((1, End::Fixed(0)))));
    }

    #[test]
    fn a_ring_point_comes_out_at_the_drawn_radius() {
        let ring = Ring { center: Point::new(10.0, 10.0), min_radius: 5.0, max_radius: 5.0 };
        let mut random = URandomState(URandom::new(1));
        let mut angle = 0.0;
        let point = place_point_on_ring(&mut random, &ring, &mut angle);
        assert!((point.distance_to(ring.center) - 5.0).abs() < 0.001, "{point:?}");
        assert_eq!(angle, RING_ANGLE_STEP);
    }

    #[test]
    fn separating_moves_both_ends_apart_but_not_a_fixed_one() {
        let ring = Ring { center: Point::default(), min_radius: 0.0, max_radius: 100.0 };
        let mut points = [Point::new(1.0, 0.0), Point::new(-1.0, 0.0)];
        separate_closest_pair(&mut points, &[], &ring, 1.0);
        assert_eq!(points, [Point::new(2.0, 0.0), Point::new(-2.0, 0.0)]);

        let fixed = [Point::new(0.0, 0.0)];
        let mut points = [Point::new(1.0, 0.0)];
        separate_closest_pair(&mut points, &fixed, &ring, 1.0);
        assert_eq!(points, [Point::new(2.0, 0.0)]);
        assert_eq!(fixed, [Point::new(0.0, 0.0)]);
    }

    #[test]
    fn separating_clamps_to_the_outer_radius() {
        let ring = Ring { center: Point::default(), min_radius: 0.0, max_radius: 2.0 };
        let mut points = [Point::new(1.9, 0.0), Point::new(-1.9, 0.0)];
        separate_closest_pair(&mut points, &[], &ring, 1.0);
        assert!((points[0].x - 2.0).abs() < 1e-5 && (points[1].x + 2.0).abs() < 1e-5, "{points:?}");
    }

    #[test]
    fn one_point_with_nothing_fixed_is_left_alone() {
        let ring = Ring { center: Point::default(), min_radius: 0.0, max_radius: 2.0 };
        let mut points = [Point::new(1.0, 0.0)];
        separate_closest_pair(&mut points, &[], &ring, 1.0);
        assert_eq!(points, [Point::new(1.0, 0.0)]);
    }

    #[test]
    fn relax_stops_at_the_minimum() {
        let ring = Ring { center: Point::default(), min_radius: 0.0, max_radius: 100.0 };
        let mut points = [Point::new(1.0, 0.0), Point::new(-1.0, 0.0)];
        let distance = relax_points(&mut points, &[], &ring, 1.0, 10.0, 1000);
        assert!(distance > 10.0);
    }

    #[test]
    fn spread_stops_when_the_average_settles() {
        let ring = Ring { center: Point::default(), min_radius: 1.0, max_radius: 1.0 };
        let mut points = [Point::new(1.0, 0.0), Point::new(-1.0, 0.0)];
        // pinned at the radius, the distance never changes: the second round settles
        let average = spread_points(&mut points, &[], &ring, 0.5, 2.0, 0.01, 50);
        assert!((average - 4.0).abs() < 0.01, "{average}");
    }
}
