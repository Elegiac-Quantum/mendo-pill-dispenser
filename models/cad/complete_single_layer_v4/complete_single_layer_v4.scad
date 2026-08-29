// Complete SmartPill single-layer printable assembly V4.
// Uses the simplified two-part medication mechanism.
use <../mg996r_as5600_gear_v1/mg996r_as5600_gear_v1.scad>
use <../two_part_carousel_v3/two_part_carousel_v3.scad>

$fn = 96;
part = is_undef(part) ? "assembly" : part;
drive_angle = is_undef(drive_angle) ? 0 : drive_angle;

deck_z = 70;
fixed_t = 4;
rotor_gap = 0.30;
rotor_z_v4 = deck_z+fixed_t+rotor_gap;
cell_h_v4 = 20;
guard_z = deck_z+fixed_t;
guard_h = 22;
m3_pilot_d_v4 = 2.6;
m2_pilot_d_v4 = 1.7;
chute_mounts_v4 = [[0,-39],[29,-39]];
chute_upper_v4 = [[5.48,-34.57],[18.03,-30.00]];
chute_lower_v4 = [[11,-56],[49,-56]];

module rounded_shell(size,r,wall,roof) {
    difference() {
        rounded_rect(size,r);
        translate([0,0,-0.1])
            rounded_rect([size[0]-2*wall,size[1]-2*wall,size[2]-roof+0.1],r-wall);
    }
}

module top_guard() {
    difference() {
        union() {
            // Thin enclosure shell with a 2 mm roof above both gears.
            rounded_shell([196,176,guard_h],10,2,2);
            // Solid stack bosses carry screw load to the fixed base and case.
            for(x=[-85,85],y=[-75,75])
                translate([x,y,0]) cylinder(d=10,h=guard_h);
        }
        // Rotor and its lift-off lid pass through this opening. The remaining
        // roof covers the outer gear teeth and the complete servo pinion.
        translate([0,0,-1]) cylinder(r=68.5,h=guard_h+2);
        for(x=[-85,85],y=[-75,75])
            translate([x,y,-1]) cylinder(d=m3_pilot_d_v4,h=guard_h+2);
    }
}

module chute_node(p,z,d=3,h=2) {
    translate([p[0],p[1],z]) cylinder(d=d,h=h);
}

module medicine_chute_v4() {
    difference() {
        union() {
            // A continuous 2 mm sloping floor covers the complete 22 degree
            // outlet and ends over the removable cup.
            hull() {
                for(p=chute_upper_v4) chute_node(p,-3.0,3.2,2.0);
                for(p=chute_lower_v4) chute_node(p,-28.5,3.2,2.0);
            }
            // Raised side rails stop round tablets leaving the slope.
            hull() {
                // The mouth stays below the fixed plate; rail height grows
                // along the slope so it cannot rub the rotating medicine tray.
                chute_node(chute_upper_v4[0],-3.0,3.2,2.6);
                chute_node(chute_lower_v4[0],-28.5,3.2,7.0);
            }
            hull() {
                chute_node(chute_upper_v4[1],-3.0,3.2,2.6);
                chute_node(chute_lower_v4[1],-28.5,3.2,7.0);
            }
            // Two mounting ears connect to the ramp and sit 0.4 mm below the
            // fixed base. M2 screws enter blind underside holes.
            for(i=[0:1]) {
                hull() {
                    chute_node(chute_mounts_v4[i],-3.0,8,2.6);
                    chute_node(chute_upper_v4[i],-3.0,4,2.6);
                }
            }
        }
        for(p=chute_mounts_v4)
            translate([p[0],p[1],-3.1]) cylinder(d=m2_pilot_d_v4,h=3.0);
    }
}

module complete_assembly() {
    color("LightGray") base();
    color("Gainsboro") translate([0,0,deck_z]) fixed_base();
    color([0.15,0.7,0.78,0.82])
        translate([0,0,rotor_z_v4])
            rotate([0,0,-drive_angle*24/136]) rotor_gear();
    color("Orange") translate([80,0,deck_z+11])
        rotate([0,0,180/24+drive_angle]) pinion();
    color([0.82,0.86,0.9,0.9]) translate([0,0,guard_z]) top_guard();
    color([0.75,0.9,0.95,0.6])
        translate([0,0,rotor_z_v4+cell_h_v4]) rotor_lid();
    color("SlateGray") translate([80,10.35,27]) servo_clamp();
    color("LightSteelBlue") translate([30,-67,7]) cup();
    color("Gainsboro") translate([-45,-92.5,38]) screen_bezel();
    color("DimGray") translate([-45,-80.5,38]) screen_back_clamp();
    color("DimGray") translate([75,-81.5,48]) button_clamp();
}

module collision_guard_drive() {
    intersection() {
        translate([0,0,guard_z]) top_guard();
        union() {
            translate([0,0,rotor_z_v4])
                rotate([0,0,-drive_angle*24/136]) rotor_gear();
            // Test the pinion at its maximum expected +4 mm horn offset.
            translate([80,0,deck_z+12]) rotate([0,0,180/24+drive_angle]) pinion();
        }
    }
}

module collision_guard_lid() {
    intersection() {
        translate([0,0,guard_z]) top_guard();
        translate([0,0,rotor_z_v4+cell_h_v4]) rotor_lid();
    }
}

module collision_chute_fixed_base() {
    intersection() {
        translate([0,0,deck_z]) fixed_base();
        translate([0,0,deck_z]) medicine_chute_v4();
    }
}

module collision_chute_enclosure() {
    intersection() {
        base();
        translate([0,0,deck_z]) medicine_chute_v4();
    }
}

module collision_chute_cup() {
    intersection() {
        translate([0,0,deck_z]) medicine_chute_v4();
        translate([30,-67,7]) cup();
    }
}

if(part=="assembly") complete_assembly();
else if(part=="top_guard") top_guard();
else if(part=="medicine_chute_v4") medicine_chute_v4();
else if(part=="collision_guard_drive") collision_guard_drive();
else if(part=="collision_guard_lid") collision_guard_lid();
else if(part=="collision_chute_fixed_base") collision_chute_fixed_base();
else if(part=="collision_chute_enclosure") collision_chute_enclosure();
else if(part=="collision_chute_cup") collision_chute_cup();
