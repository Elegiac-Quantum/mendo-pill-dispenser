// Mendo by EQ Studio. Underside engraving only; all fit geometry is preserved.
// Source: V16 base(), 200 x 180 x 71.2 mm including locating lip.
// The 0.5 mm recess leaves 1.9 mm of the original 2.4 mm floor.
use <mg996r_as5600_gear_v1/mg996r_as5600_gear_v1.scad>
difference() {
    base();
    translate([0,20,0.5]) rotate([180,0,0])
        linear_extrude(height=0.6)
            text("Mendo by EQ Studio", size=5,
                 font="Liberation Sans:style=Bold", halign="center", valign="center");
}
