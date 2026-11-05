import CoreGraphics
import Foundation
// TEST ONLY (tools/harness/posix_game.py, macOS): prints "<windowid> <x> <y> <w> <h>" of the largest layer-0 window
// owned by the process named argv[1] (default ee-aoc), for `screencapture -l <windowid>`; exit 1 when there is none.
let name = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "ee-aoc"
let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
var best: (Int, CGRect)? = nil
for w in list {
    guard let owner = w[kCGWindowOwnerName as String] as? String, owner == name,
          let id = w[kCGWindowNumber as String] as? Int,
          let b = w[kCGWindowBounds as String] as? [String: CGFloat],
          (w[kCGWindowLayer as String] as? Int) == 0 else { continue }
    let r = CGRect(x: b["X"]!, y: b["Y"]!, width: b["Width"]!, height: b["Height"]!)
    if best == nil || r.width * r.height > best!.1.width * best!.1.height { best = (id, r) }
}
if let b = best { print(b.0, Int(b.1.minX), Int(b.1.minY), Int(b.1.width), Int(b.1.height)) } else { exit(1) }
