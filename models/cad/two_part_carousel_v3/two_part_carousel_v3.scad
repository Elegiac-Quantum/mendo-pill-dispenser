// SmartPill two-part carousel V3
// Core mechanism: fixed_base + rotor_gear. The previously printed 24T pinion
// remains the only separate printed transmission interface.
use <MCAD/involute_gears.scad>

$fn = 96;
part = is_undef(part) ? "assembly" : part;
drive_angle = is_undef(drive_angle) ? 0 : drive_angle;

cells = 15;
cell_step = 360/cells;
cell_ri = 35;
cell_ro = 67;
wall_t = 1.8;
cell_h = 20;
centre_disk_t = 6.5;

gear_module = 1;
large_teeth = 136;
pinion_teeth = 24;
large_gear_t = 10;
pinion_t = 6;
large_gear_z = 8.7;
pinion_bottom_above_deck = 11.0; // measured range 10-12 mm
gear_pitch = 180*gear_module;
shaft_distance = gear_module*(large_teeth+pinion_teeth)/2; // 80 mm
pinion_phase = 180/pinion_teeth;
m3_pilot_d = 2.6;
m2_pilot_d = 1.7;

base_w = 200;
base_d = 180;
base_t = 4.0;
base_corner = 12;
running_gap = 0.30;
deck_groove_outer = [196.4,176.4]; // 0.2 mm clearance per lip side
deck_groove_inner = [192.8,172.8];
deck_groove_depth = 1.4; // 0.2 mm deeper than the 1.2 mm base tongue

// User-validated upper-shell L retainers, now integral with fixed_base.
shell_outer = [196,176];
shell_wall = 1.2;
shell_corner_r = 10;
clip_fit = 0.30;
clip_rib_outer_projection = 0.35;
clip_rib_width = 0.55; // adds 0.20 mm inward overlap; outer fit edge unchanged
clip_h = 12;
clip_foot_d = 14;
clip_seat_t = 1.6;
clip_anchor_overlap = 0.15;

// Fixed upward annular rail and matching recessed rotor groove.
rail_ri = 30.8;
rail_ro = 33.2;
rail_h = 5.3;
groove_ri = 30.4;
groove_ro = 33.6;
groove_depth = 5.0;

// User-selected 24 x 24 mm MT6701 module and supplied 4 x 2 mm magnet.
encoder_pocket = [25.0,25.0,3.2];
encoder_membrane = base_t-encoder_pocket[2]; // 0.8 mm
encoder_retainer_xy = 14.0;
magnet_d = 4.3;
magnet_depth = 2.2;

servo_axis = [shaft_distance,0];
servo_body = [40.7,19.7,36.5];
servo_axis_from_near_end = 10.0;
servo_body_cy = servo_body[0]/2-servo_axis_from_near_end;
servo_mount_dx = 10.2;
servo_mount_dy = 47.8;
horn_clear_d = 23.5;
servo_body_opening = [22.0,42.0]; // 0.65 mm clearance per long side

outlet_angle = -70;
outlet_width = cell_step-2.0;
outlet_ri = cell_ri;
outlet_ro = 65.2;
chute_mounts = [[0,-39],[29,-39]];

module rounded_rect(size, r) {
    hull()
        for(x=[-size[0]/2+r,size[0]/2-r], y=[-size[1]/2+r,size[1]/2-r])
            translate([x,y,0]) cylinder(r=r,h=size[2]);
}

module clip_rounded_footprint(size,r) {
    hull() for(x=[-size[0]/2+r,size[0]/2-r],
               y=[-size[1]/2+r,size[1]/2-r])
        translate([x,y]) circle(r=r);
}

module integrated_shell_clip(sx=1,sy=1) {
    inner = [shell_outer[0]-2*shell_wall,
             shell_outer[1]-2*shell_wall];
    inner_r = shell_corner_r-shell_wall;
    // Start 0.15 mm inside the plate, but compensate every top feature so
    // the shell engagement geometry remains at its proven original height.
    translate([sx*85,sy*75,base_t-clip_anchor_overlap])
        difference() {
            union() {
                cylinder(d=clip_foot_d,h=clip_seat_t+clip_anchor_overlap);
                linear_extrude(height=clip_seat_t+clip_anchor_overlap)
                    translate([-sx*85,-sy*75])
                        intersection() {
                            translate([sx*85,sy*75]) square([26,26],center=true);
                            offset(delta=-clip_fit)
                                clip_rounded_footprint(inner,inner_r);
                        }
                linear_extrude(height=clip_h+clip_anchor_overlap)
                    translate([-sx*85,-sy*75])
                        intersection() {
                            translate([sx*85,sy*75]) square([26,26],center=true);
                            offset(delta=-clip_fit)
                                clip_rounded_footprint(inner,inner_r);
                            union() {
                                translate([sx*(inner[0]/2-2),sy*75])
                                    square([4,26],center=true);
                                translate([sx*85,sy*(inner[1]/2-2)])
                                    square([26,4],center=true);
                            }
                        }
                translate([sx*(inner[0]/2-85-clip_fit+
                                  clip_rib_outer_projection-clip_rib_width/2),
                           0,7.5+clip_anchor_overlap])
                    cube([clip_rib_width,5,7],center=true);
                translate([0,
                           sy*(inner[1]/2-75-clip_fit+
                              clip_rib_outer_projection-clip_rib_width/2),
                           7.5+clip_anchor_overlap])
                    cube([5,clip_rib_width,7],center=true);
            }
            translate([0,0,-0.1])
                cylinder(d=3.3,h=clip_h+clip_anchor_overlap+0.2);
        }
}

module sector_cut(r1,r2,a_width,h) {
    rotate([0,0,outlet_angle])
        linear_extrude(h)
            polygon(concat(
                [for(a=[-a_width/2:a_width/16:a_width/2]) [r2*cos(a),r2*sin(a)]],
                [for(a=[a_width/2:-a_width/16:-a_width/2]) [r1*cos(a),r1*sin(a)]]));
}

module bearing_rail() {
    translate([0,0,base_t])
        difference() {
            cylinder(r=rail_ro,h=rail_h);
            translate([0,0,-0.1]) cylinder(r=rail_ri,h=rail_h+0.2);
        }
}

module fixed_base() {
    difference() {
        union() {
            rounded_rect([base_w,base_d,base_t],base_corner);
            bearing_rail();
            for(sx=[-1,1],sy=[-1,1]) integrated_shell_clip(sx,sy);
            // Blind bosses accept M2 self-tapping screws plus wide washers to
            // retain the MT6701 board from below; no printed clamp is needed.
            for(x=[-encoder_retainer_xy,encoder_retainer_xy],
                y=[-encoder_retainer_xy,encoder_retainer_xy])
                translate([x,y,0]) cylinder(d=6,h=3.2);
        }

        // One medication outlet through the fixed sliding surface.
        translate([0,0,-1]) sector_cut(outlet_ri,outlet_ro,outlet_width,base_t+2);

        // Blind underside holes retain the removable chute without exposing
        // screw heads to medication. A 1 mm roof remains above each hole.
        for(p=chute_mounts)
            translate([p[0],p[1],-0.1]) cylinder(d=m2_pilot_d,h=3.1);

        // MT6701 installs from below. The remaining 0.8 mm plastic membrane
        // protects the sensor while preserving the magnetic air gap.
        translate([-encoder_pocket[0]/2,-encoder_pocket[1]/2,-0.1])
            cube([encoder_pocket[0],encoder_pocket[1],encoder_pocket[2]+0.1]);
        // Open underside wire channel from the module pocket toward the rear.
        translate([-3,10,-0.1]) cube([6,58,2.2]);
        for(x=[-encoder_retainer_xy,encoder_retainer_xy],
            y=[-encoder_retainer_xy,encoder_retainer_xy])
            translate([x,y,-0.1]) cylinder(d=m2_pilot_d,h=3.0);

        // Full MG996R upper-body opening: the case/crown can protrude above
        // the deck while the two end mounting ears remain supported.
        translate([servo_axis[0],servo_body_cy,base_t/2])
            cube([servo_body_opening[0],servo_body_opening[1],base_t+2],center=true);
        // Symmetric mounting-ear fasteners remain just beyond the long edges.
        for(x=[-servo_mount_dx/2,servo_mount_dx/2],
            y=[-servo_mount_dy/2,servo_mount_dy/2])
            translate([servo_axis[0]+x,servo_body_cy+y,-1]) cylinder(d=m3_pilot_d,h=base_t+2);

        // Main-enclosure stack fasteners.
        for(x=[-85,85],y=[-75,75])
            translate([x,y,-1]) cylinder(d=m3_pilot_d,h=base_t+2);

        // Underside locating groove accepts the base's perimeter tongue.
        // Keep the full 1.4 mm tongue clearance, then close the groove with
        // two self-supporting slopes instead of one horizontal bridge roof.
        translate([0,0,-0.1])
            difference() {
                rounded_rect([deck_groove_outer[0],deck_groove_outer[1],
                              deck_groove_depth+0.1],10.2);
                translate([0,0,-0.1])
                    rounded_rect([deck_groove_inner[0],deck_groove_inner[1],
                                  deck_groove_depth+0.3],8.4);
            }
        difference() {
            hull() {
                translate([0,0,deck_groove_depth-0.1])
                    rounded_rect([deck_groove_outer[0],deck_groove_outer[1],0.12],10.2);
                translate([0,0,deck_groove_depth+0.9])
                    rounded_rect([194.7,174.7,0.12],9.35);
            }
            hull() {
                translate([0,0,deck_groove_depth-0.2])
                    rounded_rect([deck_groove_inner[0],deck_groove_inner[1],0.32],8.4);
                translate([0,0,deck_groove_depth+0.8])
                    rounded_rect([194.5,174.5,0.32],9.25);
            }
        }
    }
}

module large_gear() {
    gear(number_of_teeth=large_teeth,circular_pitch=gear_pitch,
         pressure_angle=20,clearance=0.25,gear_thickness=large_gear_t,
         rim_thickness=large_gear_t,rim_width=3,
         hub_thickness=large_gear_t,hub_diameter=0,bore_diameter=132);
}

module rotor_gear() {
    difference() {
        union() {
            // Every moving feature starts on the same Z=0 print plane.
            difference() {
                cylinder(r=cell_ro,h=cell_h);
                translate([0,0,-1]) cylinder(r=cell_ro-wall_t,h=cell_h+2);
            }
            difference() {
                cylinder(r=cell_ri+wall_t,h=cell_h);
                translate([0,0,-1]) cylinder(r=cell_ri,h=cell_h+2);
            }
            for(a=[0:cell_step:359])
                // Divider phase puts a cell centre, not a divider, over the
                // fixed outlet at the zero/index position for any cell count.
                rotate([0,0,a+outlet_angle-cell_step/2])
                    translate([cell_ri,-wall_t/2,0])
                        cube([cell_ro-cell_ri,wall_t,cell_h]);
            cylinder(r=cell_ri,h=centre_disk_t);
            // The gear overlaps the outer wall and is therefore one solid part.
            translate([0,0,large_gear_z]) large_gear();
        }

        // Recess receives the fixed upward rail. Radial clearance is 0.4 mm
        // per side; rail/groove depths establish a 0.30 mm running gap.
        translate([0,0,-0.1]) difference() {
            cylinder(r=groove_ro,h=groove_depth+0.1);
            translate([0,0,-0.1]) cylinder(r=groove_ri,h=groove_depth+0.3);
        }

        // The 4 x 2 mm magnet is glued flush into this centred underside pocket.
        translate([0,0,-0.1]) cylinder(d=magnet_d,h=magnet_depth+0.1);
    }
}

module pinion_reference() {
    gear(number_of_teeth=pinion_teeth,circular_pitch=gear_pitch,
         pressure_angle=20,clearance=0.25,gear_thickness=pinion_t,
         rim_thickness=pinion_t,hub_thickness=pinion_t,
         hub_diameter=20,bore_diameter=6.4);
}

module encoder_reference() {
    color("RoyalBlue") translate([0,0,encoder_pocket[2]/2])
        cube([24,24,3.2],center=true);
}

module magnet_reference() {
    color("Silver") translate([0,0,base_t+running_gap+magnet_depth/2])
        cylinder(d=4,h=magnet_depth,center=true);
}

module assembly() {
    color("Gainsboro") fixed_base();
    color([0.2,0.75,0.85,0.82])
        translate([0,0,base_t+running_gap])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_gear();
    color("Orange") translate([servo_axis[0],servo_axis[1],base_t+running_gap])
        translate([0,0,pinion_bottom_above_deck-running_gap])
            rotate([0,0,pinion_phase+drive_angle]) pinion_reference();
    encoder_reference();
    magnet_reference();
}

module parts() {
    fixed_base();
    translate([230,0,0]) rotor_gear();
}

module collision_bearing() {
    intersection() {
        fixed_base();
        translate([0,0,base_t+running_gap])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_gear();
    }
}

module collision_gears() {
    intersection() {
        translate([0,0,base_t+running_gap+large_gear_z])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) large_gear();
        translate([servo_axis[0],servo_axis[1],base_t+pinion_bottom_above_deck])
            rotate([0,0,pinion_phase+drive_angle]) pinion_reference();
    }
}

if(part=="assembly") assembly();
else if(part=="fixed_base") fixed_base();
else if(part=="rotor_gear") rotor_gear();
else if(part=="parts") parts();
else if(part=="collision_bearing") collision_bearing();
else if(part=="collision_gears") collision_gears();

echo(parts=2,cells=cells,gear_ratio=large_teeth/pinion_teeth,
     servo_degrees_per_cell=cell_step*large_teeth/pinion_teeth,
     radial_clearance_each_side=(rail_ri-groove_ri),
     running_gap=running_gap,
     nominal_magnet_sensor_gap=base_t+running_gap-encoder_pocket[2],
     large_gear_above_deck=[running_gap+large_gear_z,
                            running_gap+large_gear_z+large_gear_t],
     pinion_above_deck=[pinion_bottom_above_deck,
                        pinion_bottom_above_deck+pinion_t]);
