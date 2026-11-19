//! cls_carrier.rs's tests: the flight plans over cls_move.rs' mock world (its carrier, tag, command, random and
//! reach fields), each checked against the C++ body's statements worked by hand.

use super::*;
use crate::units::cls_move::k::MV_GOAL_LOCATION;
use crate::units::cls_move::tests::{Mock, MockGoal};

impl CarrierHost for Mock {
    fn stats(&mut self, unit: u64) -> *const EeUnitsUnitClassUpgradeFields {
        &*self.u(unit).stats
    }
    fn home_base(&mut self, unit: u64) -> u64 {
        self.home.get(&unit).copied().unwrap_or(0)
    }
    fn tag(&mut self, carrier: u64, _plane: u64, index: i32) -> Option<Ge3DPoint> {
        self.tags.get(&(carrier, index)).copied()
    }
    fn set_orientation(&mut self, _unit: u64, o: GeOrientation) {
        self.say(format!("orientation {}", o.yaw));
    }
    fn command_type(&mut self, _unit: u64) -> i32 {
        self.command
    }
    fn set_position(&mut self, unit: u64, p: Ge3DPoint) {
        self.u(unit).location = [p.x, p.y, p.z];
        self.say(format!("position {} {} {}", p.x, p.y, p.z));
    }
    fn set_scale(&mut self, _unit: u64, s: f32) {
        self.say(format!("scale {s}"));
    }
    fn route_dock_waypoint(&mut self, _class: u64, unit: u64) {
        route_dock_waypoint(self, unit);
    }
    fn fly_to_goal(&mut self, _class: u64, p: Ge3DPoint, _unit: u64) {
        self.say(format!("fly {} {} {}", p.x, p.y, p.z));
    }
    fn has_air_traffic(&mut self, _unit: u64) -> bool {
        true
    }
    fn deck_goal(&mut self, _unit: u64, deck: Ge3DPoint) {
        self.say(format!("deck goal {} {} {}", deck.x, deck.y, deck.z));
    }
    fn location_goal_point(&mut self, unit: u64) -> Ge3DPoint {
        let g = self.u(unit).goal;
        self.g(g).location
    }
    fn home_first_slot(&mut self, unit: u64) -> u64 {
        if let Some(&a) = self.home.get(&0) {
            self.home.insert(unit, a);
            a
        } else {
            self.say("killed".into());
            0
        }
    }
    fn next_waypoint_at(&mut self, unit: u64) -> Option<Ge3DPoint> {
        self.u(unit).waypoints.first().copied()
    }
    fn random(&mut self) -> f32 {
        self.say("random".into());
        self.draw
    }
    fn height_at(&mut self, _class: u64, _unit: u64, p: Ge3DPoint) -> f32 {
        p.z + 1.0
    }
    fn is_space_map(&mut self) -> bool {
        self.space
    }
    fn can_get_to(&mut self, _unit: u64, x: i32, y: i32) -> bool {
        self.reach.contains(&[x, y])
    }
}

fn carrier_world() -> (Mock, u64, u64, u64) {
    let mut m = Mock::map(40, 40);
    let c = m.class(|c| c.mCruiseAltitude = 4.0);
    let plane = m.add(10.5, 10.5, c);
    let carrier = m.add(20.5, 20.5, c);
    m.home.insert(plane, carrier);
    m.u(plane).stats.mMaxVelocity = 2.0;
    m.u(plane).stats.mMaxAngularVelocity = 1.0;
    m.u(plane).stats.mScale = 2.0;
    (m, c, plane, carrier)
}

#[test]
fn route_dock_takes_tag_two_or_the_centre() {
    let (mut m, _c, plane, carrier) = carrier_world();
    route_dock_waypoint(&mut m, plane);
    assert_eq!(m.log, ["drop_path", "waypoint 20.5 20.5 1.5 true"]);
    m.tags.insert((carrier, 2), at(21.0, 22.0, 0.25));
    m.log.clear();
    route_dock_waypoint(&mut m, plane);
    assert_eq!(m.log, ["drop_path", "waypoint 21 22 0.75 true"]);
}

#[test]
fn return_to_airport_rolls_then_climbs() {
    let (mut m, c, plane, _carrier) = carrier_world();
    assert!(return_to_airport(&mut m, c, plane));
    // identity orientation: 3 and 7 ahead along y; ground 0 (no tile in the mock's records); 3/4 of cruise 4
    assert_eq!(
        m.log,
        ["drop_path", "waypoint 10.5 13.5 0.5 true", "waypoint 10.5 17.5 3.5 true", "orientation 0", "fly 10.5 17.5 3"]
    );
}

#[test]
fn approach_carrier_plans_the_deck() {
    let (mut m, c, plane, carrier) = carrier_world();
    // no tags: only the path dropped
    approach_carrier(&mut m, c, plane, true);
    assert_eq!(m.log, ["drop_path"]);
    m.tags.insert((carrier, 0), at(20.0, 20.0, 0.5));
    m.tags.insert((carrier, 2), at(20.0, 21.0, 0.5));
    m.log.clear();
    m.u(plane).location = [10.5, 10.5, 1.0];
    approach_carrier(&mut m, c, plane, false);
    // height 1 below cruise and below the glide point: the deck, then the carrier
    assert_eq!(m.log, ["drop_path", "waypoint 20 20 1 true", "waypoint 20.5 20.5 0.5 true", "deck goal 20 20 0.5"]);
    m.command = ACTION_BRAKE;
    m.log.clear();
    approach_carrier(&mut m, c, plane, false);
    assert_eq!(m.log, ["drop_path", "waypoint 20 21 1 true", "orientation 0", "deck goal 20 21 0.5"]);
}

#[test]
fn take_off_from_the_deck() {
    let (mut m, c, plane, carrier) = carrier_world();
    m.tags.insert((carrier, 0), at(20.0, 19.0, 1.0));
    m.tags.insert((carrier, 2), at(20.0, 22.0, 1.0));
    m.command = ACTION_TAKEOFF;
    take_off_from_carrier(&mut m, c, plane);
    // the deck start is low: scale 2 * (1 / 4 * 0.5 + 0.5); then the dock waypoint (tag 2)
    assert_eq!(m.log, ["drop_path", "position 20 19 1", "scale 1.25", "drop_path", "waypoint 20 22 1.5 true"]);
    m.command = 0;
    m.log.clear();
    take_off_from_carrier(&mut m, c, plane);
    assert_eq!(m.log, ["drop_path", "waypoint 20 32 3.3 true", "fly 20 32 1"]);
}

#[test]
fn away_from_goal_beyond_one() {
    let (mut m, _c, plane, carrier) = carrier_world();
    let g = m.goal(MockGoal { kind: MV_GOAL_LOCATION, location: at(20.5, 21.0, 1.0), ..Default::default() });
    m.u(plane).goal = g;
    assert!(!is_away_from_goal(&mut m, plane));
    m.tags.insert((carrier, 0), at(23.0, 20.0, 1.0));
    assert!(is_away_from_goal(&mut m, plane));
}

#[test]
fn circle_side_on_a_space_map_goes_where_it_can_reach() {
    let (mut m, _c, plane, _carrier) = carrier_world();
    m.draw = 0.7;
    circle_side(&mut m, plane);
    assert_eq!(m.u(plane).cx.mCurrentPhysicsOrder, 1);
    m.space = true;
    // heading 0 (+y): left is -x, distance ceil(2 / 1) = 2: (10.5 + 2, 10.5) truncated is (12, 10)
    m.reach.push([12, 10]);
    m.log.clear();
    circle_side(&mut m, plane);
    assert_eq!(m.u(plane).cx.mCurrentPhysicsOrder, 0);
    assert!(m.log.is_empty());
}

#[test]
fn holding_pattern_over_the_home_airport() {
    let (mut m, c, plane, carrier) = carrier_world();
    m.home.clear();
    holding_pattern(&mut m, c, plane);
    assert_eq!(m.log, ["killed"]);
    m.home.insert(0, carrier);
    m.u(plane).cx.mIsCommandable = true;
    m.log.clear();
    m.draw = 0.2;
    holding_pattern(&mut m, c, plane);
    assert!(!m.u(plane).cx.mIsCommandable);
    assert_eq!(m.u(plane).cx.mCurrentPhysicsOrder, 0);
    assert_eq!(m.log[0], "drop_path");
    let height = m.u(plane).waypoints[0].z + 1.0;
    assert_eq!(m.u(plane).cx.mHeight, height);
    assert_eq!(um_ceil(2.0), 2);
    assert_eq!(um_ceil(2.1), 3);
    assert_eq!(um_ceil(-0.5), 0);
}
