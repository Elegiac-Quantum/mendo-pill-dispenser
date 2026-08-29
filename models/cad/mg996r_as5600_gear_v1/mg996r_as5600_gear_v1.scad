// SmartPill 18-cell carousel - MG996R continuous servo + MT6701 feedback.
// DESIGN REVIEW MODEL: do not print before the validation report is complete.
// Requires the MCAD library distributed with OpenSCAD.
use <MCAD/involute_gears.scad>

$fn = 72;
part = is_undef(part) ? "assembly" : part;
drive_angle = is_undef(drive_angle) ? 0 : drive_angle;
cup_pull = is_undef(cup_pull) ? 0 : cup_pull;
// assembly, parts_plate, base, deck, medication_floor, rotor, rotor_drive, large_gear, pinion, cup, chute,
// servo_clamp, screen_bezel, collision_pinion_rotor, collision_gears,
// collision_gear_outlet, collision_gear_deck, collision_servo_board,
// collision_chute_cup, collision_cup_base, collision_rotor_deck,
// collision_drive_cover, collision_chute_base, collision_servo_cradle_board,
// collision_servo_cradle_base, collision_servo_fasteners_drive

cells = 18;
cell_step = 360/cells;
rotor_ri = 35;
rotor_ro = 67;
rotor_wall = 1.8;
rotor_floor = 2.2;
cell_h = 20;

gear_module = 1;
// 24/136 keeps the original 80 mm shaft spacing while leaving enough tooth
// root around the supplied horn's 14.5 mm screw circle.
large_teeth = 136; // external ring below the non-medication outer wall
pinion_teeth = 24;
gear_thickness = 6;
gear_pitch = 180*gear_module;
gear_center_distance = gear_module*(large_teeth+pinion_teeth)/2; // 80 mm
pinion_phase = 180/pinion_teeth;

base_w = 200;
base_d = 180;
corner_r = 12;
base_h = 70;
wall = 2.4;
deck_z = base_h;
deck_t = 3;
gear_z = deck_z+deck_t+1.0;
rotor_z = gear_z+gear_thickness+0.8;
gear_cover_z = deck_z+deck_t+0.2;
gear_cover_h = 9.0;
floor_gap = 0.7;
fixed_floor_t = 2.0;
fixed_floor_top = rotor_z-deck_z-floor_gap;
floor_mount_r = 39;
floor_mount_angles = [20,140,220];
bearing_tower_ri = 29.5;
bearing_tower_ro = 34.3;
bearing_groove_ri = 30.7;
bearing_groove_ro = 33.3;
bearing_boss_ri = 31.1;
bearing_boss_ro = 32.9;
bearing_boss_h = 1.5;

// Classic TowerPro MG996R dimensions. The 42.9 mm published height includes
// the output spline; the rectangular case below the crown is about 36.5 mm.
servo_body = [40.7,19.7,36.5];
servo_total_h = 42.9;
servo_axis = [gear_center_distance,0];
servo_clearance = 0.7;
servo_axis_from_near_end = 10.0;
servo_body_cy = servo_axis[1] + servo_body[0]/2-servo_axis_from_near_end;
// Supplied MG996R round plastic horn, measured from the user's 3MF.
servo_horn_d = 21.0;
servo_horn_h = 6.26;
servo_horn_pcd = 14.5;
m3_pilot_d = 2.6; // user-validated M3 self-tapping hole
m2_pilot_d = 1.7; // user-validated M2 self-tapping hole
servo_horn_link_hole_d = m2_pilot_d;
servo_mount_dx = 10.2;
servo_mount_dy = 47.8;

outlet_angle = -70;
outlet_inner = rotor_ri;
outlet_outer = 65.1;
outlet_angle_width = 16.0;

// User-selected MT6701 module: 24 mm square, supplied 4 x 2 mm magnet.
encoder_board = [24,24,3.2];
magnet_d = 4.3;
magnet_h = 2.2;
screen_active = [48.96,36.72]; // 2.4-inch ST7789 landscape active area
screen_board = [61,43,6]; // conservative TL024/ATK-MD0240 module envelope
screen_x = -43;
screen_z = 38;
main_board = [120,62,15];
main_board_x = -20;
main_board_y = 18; // clears the cup system and leaves a service gap to rear boards
pca_board = [62.5,25.4,35]; // includes vertical 4700 uF capacitor and plugs
pca_center = [-43,69];
pca_hole_dx = 55.5; // user-validated hole-centre spacing
pca_hole_dy = 19.0;
rtc_board = [38.5,21.7,14];
rtc_center = [25,70];
main_hole_dx = 112.5; // user-validated DNESP32S3 hole-centre spacing
main_hole_dy = 55.5;
rtc_hole_dx = 26.0; // user-validated DS3231 hole-centre spacing
rtc_hole_dy = 17.5;
board_standoff_h = 11.0; // common 11 mm support height for all three boards
cup_x = 30;
button_x = 55; // four-hole button mount clears the front-right structural pillar
button_z = 55; // moved up so the lower M2 boss clears the cup opening
button_hole_d = 13.6; // extra 0.15 mm radial clearance for reliable return
button_board = [20.0,13.0,1.6];
button_board_cx = 3.0; // button centre is 7 mm from the PCB's left edge
button_fit = 0.35;
button_guide_t = 1.2;
button_seat_h = 4.5; // 3 mm switch body + 1.6 mm PCB - 0.1 mm preload
button_screw_xs = [-13.0,16.5];
button_screw_z = 9.5;
button_boss_d = 6.0;
deck_lip_outer = [196.0,176.0];
deck_lip_width = 1.4;
deck_lip_h = 1.2;

module rounded_box(size=[10,10,10],r=2) {
    linear_extrude(size[2]) offset(r=r) square([size[0]-2*r,size[1]-2*r],center=true);
}

module annular_sector(r1,r2,a,h) {
    rotate([0,0,-a/2]) rotate_extrude(angle=a)
        translate([r1,0]) square([r2-r1,h]);
}

module ring_sector(r1,r2,a_width,h) {
    linear_extrude(h)
        polygon(concat(
            [for(a=[-a_width/2:a_width/12:a_width/2]) [r2*cos(a),r2*sin(a)]],
            [for(a=[a_width/2:-a_width/12:-a_width/2]) [r1*cos(a),r1*sin(a)]]));
}

module sector_cut(r1,r2,a_width,h=10) {
    rotate([0,0,outlet_angle])
        linear_extrude(h)
            polygon(concat(
                [for(a=[-a_width/2:a_width/12:a_width/2])
                    [r2*cos(a),r2*sin(a)]],
                [for(a=[a_width/2:-a_width/12:-a_width/2])
                    [r1*cos(a),r1*sin(a)]]));
}

module outlet_cut(h=10) {
    // Cut slightly past the floor rim so Boolean subtraction cannot leave
    // tangent zero-width mesh fragments at the circular outer boundary.
    sector_cut(outlet_inner,outlet_outer+0.4,outlet_angle_width,h);
}

module base_shell() {
    difference() {
        rounded_box([base_w,base_d,base_h],corner_r);
        translate([0,0,wall]) rounded_box([base_w-2*wall,base_d-2*wall,base_h],corner_r-wall);
        // Front removable cup opening. Its lower edge starts exactly at the
        // 2.4 mm floor top, leaving a continuous full-thickness floor instead
        // of the former isolated 1 mm lip below the drawer.
        translate([cup_x,-base_d/2-1,(39+wall)/2])
            cube([65,35,39-wall],center=true);
        // Portrait ATK-MD0240 screen opening on the front-left face.
        translate([screen_x,-base_d/2,screen_z])
            cube([screen_active[0]+1,wall+3,screen_active[1]+1],center=true);
        // Screen sandwich-frame fasteners, independent of display PCB holes.
        for(x=[-34,34],z=[-25,25])
            translate([screen_x+x,-base_d/2,screen_z+z])
                rotate([90,0,0]) cylinder(d=m3_pilot_d,h=wall+4,center=true);
        // Front confirmation button opening; the holeless PCB uses the
        // integrated locating cage and removable V7 M2 back clamp.
        translate([button_x,-base_d/2,button_z])
            rotate([90,0,0]) cylinder(d=button_hole_d,h=wall+4,center=true);
        // Rear PG7 cable-gland opening for the external regulated 5 V input.
        translate([72,base_d/2,25])
            rotate([90,0,0]) cylinder(d=13,h=wall+4,center=true);
        // Side walls stay closed. USB/Type-C wiring uses the rear round port;
        // servo service is from the removable deck.
    }
}

module integrated_button_mount() {
    cavity_w = button_board[0]+button_fit;
    cavity_h = button_board[1]+button_fit;
    left = button_board_cx-cavity_w/2;
    right = button_board_cx+cavity_w/2;
    bottom = -cavity_h/2;
    top = cavity_h/2;
    inner_y = -base_d/2+wall;
    anchor_overlap = 0.15;
    anchored_depth = button_seat_h+anchor_overlap;

    // Left, upper and lower locating edges. The PCB's right side stays open
    // for the three pins and their connector.
    translate([button_x+left-button_guide_t/2,
               inner_y-anchor_overlap+anchored_depth/2,button_z])
        cube([button_guide_t,anchored_depth,
              cavity_h+2*button_guide_t],center=true);
    for(z=[bottom-button_guide_t/2,top+button_guide_t/2])
        translate([button_x+(left+right)/2,
                   inner_y-anchor_overlap+anchored_depth/2,button_z+z])
            cube([right-left,anchored_depth,button_guide_t],center=true);

    // Four blind M2 bosses match the removable four-point back clamp.
    for(x=button_screw_xs,z=[-button_screw_z,button_screw_z])
        translate([button_x+x,inner_y-anchor_overlap,button_z+z])
            rotate([-90,0,0]) cylinder(d=button_boss_d,h=anchored_depth);
}

module deck_locating_lip() {
    // Inner-edge tongue overlaps the enclosure wall by 0.4 mm and is fully
    // hidden inside the matching underside groove of the fixed upper plate.
    translate([0,0,base_h-0.1])
        difference() {
            rounded_box([deck_lip_outer[0],deck_lip_outer[1],deck_lip_h+0.1],10);
            translate([0,0,-0.1])
                rounded_box([deck_lip_outer[0]-2*deck_lip_width,
                             deck_lip_outer[1]-2*deck_lip_width,
                             deck_lip_h+0.3],10-deck_lip_width);
        }
}

module deck_lip_support_chamfer() {
    // Support the inward-projecting locating tongue from the enclosure wall
    // with a self-supporting taper. This removes the slicer's full perimeter
    // support ring without changing the tongue, groove or overall height.
    lower_inner = [base_w-2*wall,base_d-2*wall];
    // Extend 0.2 mm inward under the tongue to avoid a coplanar Boolean seam.
    upper_inner = [deck_lip_outer[0]-2*deck_lip_width-0.4,
                   deck_lip_outer[1]-2*deck_lip_width-0.4];
    translate([0,0,base_h-1.5])
        difference() {
            rounded_box([deck_lip_outer[0],deck_lip_outer[1],1.50],10);
            // Subtract one continuous tapered cavity; hulling annular rings
            // directly would incorrectly fill their central opening.
            hull() {
                translate([0,0,-0.1])
                    rounded_box([lower_inner[0],lower_inner[1],0.12],
                                corner_r-wall);
                translate([0,0,1.40])
                    rounded_box([upper_inner[0],upper_inner[1],0.22],
                                10-deck_lip_width);
            }
        }
}

module base() {
    difference() {
        union() {
            base_shell();
            integrated_button_mount();
            deck_lip_support_chamfer();
            deck_locating_lip();
            // Four serviceable insert bosses tie the deck and gear cover to base.
            for(x=[-85,85],y=[-75,75])
                translate([x,y,wall]) cylinder(d=10,h=base_h-wall);
            // Direct-drop cup drawer: long runners carry the enlarged cup
            // directly below the complete medication outlet.
            for(x=[cup_x-19.5,cup_x+19.5])
                translate([x,-55.5,wall]) rounded_box([6,69,4.4],1.5);
            for(x=[cup_x-30.8,cup_x+30.8])
                translate([x,-55.5,wall]) rounded_box([3,69,8],1.0);
            translate([cup_x,-19,wall]) rounded_box([57,3,8],1.0);
            // User-validated mounting patterns. Board outlines have at least
            // 7 mm clearance and remain clear of the cup and servo regions.
            for(x=[-main_hole_dx/2,main_hole_dx/2],y=[-main_hole_dy/2,main_hole_dy/2])
                translate([main_board_x+x,main_board_y+y,wall])
                    cylinder(d=7,h=board_standoff_h);
            for(x=[-pca_hole_dx/2,pca_hole_dx/2],y=[-pca_hole_dy/2,pca_hole_dy/2])
                translate([pca_center[0]+x,pca_center[1]+y,wall])
                    cylinder(d=6,h=board_standoff_h);
            for(x=[-rtc_hole_dx/2,rtc_hole_dx/2],y=[-rtc_hole_dy/2,rtc_hole_dy/2])
                translate([rtc_center[0]+x,rtc_center[1]+y,wall])
                    cylinder(d=7,h=board_standoff_h);
        }
        // User-validated self-tapping pilot holes; no heat-set inserts required.
        for(x=[-85,85],y=[-75,75])
            translate([x,y,base_h-6]) cylinder(d=m3_pilot_d,h=8);
        for(x=[-main_hole_dx/2,main_hole_dx/2],y=[-main_hole_dy/2,main_hole_dy/2])
            translate([main_board_x+x,main_board_y+y,wall+1])
                cylinder(d=m3_pilot_d,h=board_standoff_h);
        for(x=[-pca_hole_dx/2,pca_hole_dx/2],y=[-pca_hole_dy/2,pca_hole_dy/2])
            translate([pca_center[0]+x,pca_center[1]+y,wall+1])
                cylinder(d=m2_pilot_d,h=board_standoff_h);
        for(x=[-rtc_hole_dx/2,rtc_hole_dx/2],y=[-rtc_hole_dy/2,rtc_hole_dy/2])
            translate([rtc_center[0]+x,rtc_center[1]+y,wall+1])
                cylinder(d=m3_pilot_d,h=board_standoff_h);
        // Four blind thread paths for the removable button clamp. These stop
        // 0.8 mm short of the front surface; use M2 x 6 mm screws.
        for(x=button_screw_xs,z=[-button_screw_z,button_screw_z])
            translate([button_x+x,-base_d/2+0.8,button_z+z])
                rotate([-90,0,0])
                    cylinder(d=m2_pilot_d,h=wall+button_seat_h-0.6);
    }
}

module glide_and_guide() {
    // Three socketed pillars support and locate the separately printed floor.
    for(a=floor_mount_angles) rotate([0,0,a]) translate([floor_mount_r,0,deck_t-0.1])
        difference() {
            cylinder(d=10,h=fixed_floor_top-fixed_floor_t-deck_t+0.1);
            translate([0,0,-0.2]) cylinder(d=m3_pilot_d,h=8);
            translate([0,0,2.0]) cylinder(d=7.4,h=5);
        }
    // Continuous annular bearing tower: its top rails carry the centre disc,
    // while the recessed groove locates the matching rotating boss radially.
    difference() {
        translate([0,0,deck_t-0.1]) difference() {
            cylinder(r=bearing_tower_ro,h=fixed_floor_top+floor_gap-deck_t);
            translate([0,0,-0.1])
                cylinder(r=bearing_tower_ri,h=fixed_floor_top+floor_gap-deck_t+0.2);
        }
        translate([0,0,fixed_floor_top+floor_gap-2.0]) difference() {
            cylinder(r=bearing_groove_ro,h=2.2);
            translate([0,0,-0.1]) cylinder(r=bearing_groove_ri,h=2.4);
        }
    }
}

module gear_cover() {
    // Fixed top cover hides both gears. Its 68 mm centre opening provides a
    // 1 mm radial guide clearance around the 67 mm rotor outer wall.
    difference() {
        rounded_box([base_w-2*wall,base_d-2*wall,gear_cover_h],corner_r-wall);
        // Through-opening for the medication rotor.
        translate([0,0,-1]) cylinder(r=68.8,h=gear_cover_h+2);
        // Underside running cavity for the large gear and its flange.
        translate([0,0,-1]) cylinder(r=73.0,h=8.2);
        // Underside running cavity for the servo pinion.
        translate([servo_axis[0],servo_axis[1],-1]) cylinder(r=13.5,h=8.2);
        for(x=[-85,85],y=[-75,75])
            translate([x,y,-1]) cylinder(d=m3_pilot_d,h=gear_cover_h+2);
        for(x=[-servo_mount_dx/2,servo_mount_dx/2],
            y=[-servo_mount_dy/2,servo_mount_dy/2])
            translate([servo_axis[0]+x,servo_body_cy+y,-1])
                cylinder(d=m3_pilot_d,h=gear_cover_h+2);
    }
}

module medication_floor() {
    difference() {
        union() {
            difference() {
            cylinder(r=65.1,h=fixed_floor_t);
            translate([0,0,-1]) cylinder(r=rotor_ri-0.3,h=fixed_floor_t+2);
            translate([0,0,-1]) outlet_cut(fixed_floor_t+2);
            }
            // Locating bosses point downward in assembly. Print this part with
            // the smooth medication surface against the build plate.
            for(a=floor_mount_angles)
                rotate([0,0,a]) translate([floor_mount_r,0,-3]) cylinder(d=7,h=3);
        }
        // Blind insert pockets open from below and leave 0.5 mm of solid top.
        for(a=floor_mount_angles)
            rotate([0,0,a]) translate([floor_mount_r,0,-3.1]) cylinder(d=m3_pilot_d,h=4.6);
    }
}

module outlet_well() {
    well_h = fixed_floor_top-fixed_floor_t-deck_t+0.4;
    translate([0,0,deck_t-0.1])
        difference() {
            // Stop 0.3 mm inside the floor rim to avoid zero-width fragments at
            // the circular boundary. The outer drop already lands in the cup.
            sector_cut(outlet_inner-1.2,outlet_outer-0.3,outlet_angle_width+2.0,well_h);
            translate([0,0,-0.1]) outlet_cut(well_h+0.2);
        }
}

module encoder_retainer() {
    // Supports and threaded bosses accept a removable MT6701 clamp frame.
    for(x=[-9,9],y=[-9,9])
        translate([x,y,deck_t-0.1]) cylinder(d=4,h=0.6);
    for(x=[-14,14],y=[-14,14])
        translate([x,y,deck_t-0.2]) difference() {
            cylinder(d=6,h=5.9);
            translate([0,0,1]) cylinder(d=2.2,h=6);
        }
}

module chute_mount_bosses() {
    for(p=[[5.2,-29.5],[15,-26]])
        translate([p[0],p[1],deck_t-0.2]) difference() {
            cylinder(d=9,h=4.2);
            translate([0,0,-1]) cylinder(d=m2_pilot_d,h=6.2);
        }
}

module deck() {
    union() {
        difference() {
            rounded_box([base_w-2*wall,base_d-2*wall,deck_t],corner_r-wall);
            // Complete gravity path; no gate.
            translate([0,0,-1]) outlet_cut(deck_t+2);
            // Clearance for the supplied 21 mm round horn plus print/runout margin.
            translate([servo_axis[0],servo_axis[1],-1]) cylinder(d=23.5,h=deck_t+2);
            // The body is offset from its output shaft, as on a standard-size servo.
            translate([servo_axis[0],servo_body_cy,-base_h/2])
                cube([servo_body[1]+2*servo_clearance,
                      servo_body[0]+2*servo_clearance,base_h],center=true);
            for(x=[-85,85],y=[-75,75])
                translate([x,y,-1]) cylinder(d=m3_pilot_d,h=deck_t+2);
            for(a=floor_mount_angles) rotate([0,0,a]) translate([floor_mount_r,0,0]) {
                translate([0,0,-1]) cylinder(d=m3_pilot_d,h=deck_t+2);
                // Underside recess keeps an M3 low-profile head below flush.
                translate([0,0,-0.1]) cylinder(d=6.5,h=2.5);
            }
            for(p=[[5.2,-29.5],[15,-26]])
                translate([p[0],p[1],-1]) cylinder(d=m2_pilot_d,h=deck_t+2);
            for(x=[-servo_mount_dx/2,servo_mount_dx/2],
                y=[-servo_mount_dy/2,servo_mount_dy/2])
                translate([servo_axis[0]+x,servo_body_cy+y,-1])
                    cylinder(d=m3_pilot_d,h=deck_t+2);
        }
        glide_and_guide();
        encoder_retainer();
        chute_mount_bosses();
        outlet_well();
    }
}

module servo_clamp() {
    // Full body cradle: independent of clone-specific servo-ear hole spacing.
    difference() {
        union() {
            translate([0,0,21.5]) cube([25,46,43],center=true);
            translate([0,0,41.5]) cube([34,58,3],center=true);
        }
        // Body clearance; open at top for insertion.
        translate([0,0,23.5]) cube([21.6,42.1,43],center=true);
        translate([0,0,42]) cube([22,43,5],center=true);
        // Classic MG996R symmetric 10.2 x 47.8 mm mounting pattern.
        for(x=[-servo_mount_dx/2,servo_mount_dx/2],
            y=[-servo_mount_dy/2,servo_mount_dy/2])
            translate([x,y,39]) cylinder(d=m3_pilot_d,h=7);
        // Rear cable slot rises to the underside of the mounting flange while
        // leaving the screw-hole rim continuous and fully supported.
        translate([0,23,22]) cube([10,8,36],center=true);
    }
}

module board_edge_clamp() {
    difference() {
        rounded_box([12,7,2.4],2);
        translate([0,0,-1]) cylinder(d=m3_pilot_d,h=5);
    }
}

module encoder_clamp() {
    difference() {
        cube([32,32,2],center=true);
        cube([20,20,4],center=true);
        for(x=[-14,14],y=[-14,14])
            translate([x,y,-2]) cylinder(d=m2_pilot_d,h=4);
    }
}

module large_gear() {
    difference() {
        union() {
            gear(number_of_teeth=large_teeth,circular_pitch=gear_pitch,
                 pressure_angle=20,clearance=0.25,gear_thickness=gear_thickness,
                 rim_thickness=gear_thickness,rim_width=3,
                 hub_thickness=gear_thickness,hub_diameter=0,bore_diameter=132);
            // Thin attachment flange sits only below the rotor outer wall.
            translate([0,0,gear_thickness-0.2]) difference() {
                cylinder(r=68,h=1.2);
                translate([0,0,-0.1]) cylinder(r=65.4,h=1.4);
            }
            // External locating lip centres the separately printed rotor. The
            // annular interface is bonded outside the medication volume.
            translate([0,0,gear_thickness+0.8]) difference() {
                cylinder(r=68.0,h=1.5);
                translate([0,0,-0.1]) cylinder(r=67.3,h=1.7);
            }
            // Integral structural neck overlaps the rotor outer wall after
            // rotor_drive() positions this gear 6.8 mm below the cells.
            translate([0,0,gear_thickness+0.8]) difference() {
                cylinder(r=67.2,h=3.0);
                translate([0,0,-0.1]) cylinder(r=65.0,h=3.2);
            }
        }
    }
}

module pinion() {
    difference() {
        gear(number_of_teeth=pinion_teeth,circular_pitch=gear_pitch,
             pressure_angle=20,clearance=0.25,gear_thickness=gear_thickness,
             rim_thickness=gear_thickness,hub_thickness=gear_thickness,
             hub_diameter=20,bore_diameter=6.4);
        // Four M2 self-tapping screws use the cardinal holes in the supplied
        // plastic round horn. The original center screw remains accessible.
        for(a=[0:90:270]) rotate([0,0,a]) translate([servo_horn_pcd/2,0,-1]) {
            cylinder(d=2.2,h=gear_thickness+2);
            translate([0,0,3.8]) cylinder(d=4.4,h=gear_thickness);
        }
    }
}

module rotor() {
    difference() {
        union() {
            // Bottomless cells slide 0.4 mm above the fixed medication floor.
            difference() {
                cylinder(r=rotor_ro,h=cell_h);
                translate([0,0,-1]) cylinder(r=rotor_ro-rotor_wall,h=cell_h+2);
            }
            difference() {
                cylinder(r=rotor_ri+rotor_wall,h=cell_h);
                translate([0,0,-1]) cylinder(r=rotor_ri,h=cell_h+2);
            }
            for(a=[0:cell_step:359])
                rotate([0,0,a+cell_step/2])
                    translate([rotor_ri,-rotor_wall/2,0])
                        cube([rotor_ro-rotor_ri,rotor_wall,cell_h]);
            // The centre disk is outside medication space and carries magnet.
            cylinder(r=rotor_ri,h=rotor_floor);
            cylinder(d=12,h=rotor_floor+3);
            // Magnet boss points down toward the top-mounted MT6701.
            translate([0,0,-2]) cylinder(d=8,h=2+rotor_floor);
            // Annular journal boss enters the fixed deck groove. It leaves the
            // MT6701/magnet air-gap area completely open at the centre.
            translate([0,0,-bearing_boss_h]) difference() {
                cylinder(r=bearing_boss_ro,h=bearing_boss_h);
                translate([0,0,-0.1]) cylinder(r=bearing_boss_ri,h=bearing_boss_h+0.2);
            }
        }
        translate([0,0,-2.1]) cylinder(d=magnet_d,h=magnet_h+0.2);
    }
}

module rotor_drive() {
    // Single printable moving part: the annular neck overlaps the rotor outer
    // wall, so no adhesive or post-print gear alignment is required.
    union() {
        rotor();
        translate([0,0,-(gear_thickness+0.8)]) large_gear();
    }
}

module rotor_lid() {
    // Lift-off washable dust lid. The downward outer lip centres it on rotor.
    union() {
        cylinder(r=67.6,h=2.0);
        translate([0,0,-1.8]) difference() {
            cylinder(r=68.0,h=2.0);
            translate([0,0,-0.1]) cylinder(r=67.3,h=2.2);
        }
        cylinder(d=28,h=7);
    }
}

module as5600_reference() {
    color("RoyalBlue") translate([0,0,deck_z+deck_t+0.5+encoder_board[2]/2])
        cube(encoder_board,center=true);
    color("Silver") translate([0,0,rotor_z-2]) cylinder(d=4,h=2);
}

module servo_reference() {
    color([0.1,0.1,0.1,0.75])
        translate([servo_axis[0],servo_body_cy,
                   gear_z-servo_total_h+servo_body[2]/2])
            cube([servo_body[1],servo_body[0],servo_body[2]],center=true);
    color("Silver") translate([servo_axis[0],servo_axis[1],gear_z-servo_horn_h])
        cylinder(d=servo_horn_d,h=servo_horn_h);
}

module chute() {
    // Funnel ramp spans both inner corners of the 16-degree outlet and widens
    // into the cup. The outer outlet area already drops directly into the cup.
    color("BurlyWood") {
        hull() {
            translate([7.0,-34.3,deck_z-3.5]) cube([2,2,1.6],center=true);
            translate([16.6,-30.8,deck_z-3.5]) cube([2,2,1.6],center=true);
            translate([13,-56,41.5]) cube([2,2,1.6],center=true);
            translate([47,-56,41.5]) cube([2,2,1.6],center=true);
        }
        // Two M3 tabs fasten the separate chute to reinforced deck bosses.
        difference() {
            hull() {
                translate([5.2,-29.5,deck_z-4]) cylinder(d=9,h=4);
                translate([7.0,-34.3,deck_z-4]) cylinder(d=5,h=4);
            }
            translate([5.2,-29.5,deck_z-5]) cylinder(d=m3_pilot_d,h=6);
        }
        difference() {
            hull() {
                translate([15,-26,deck_z-4]) cylinder(d=9,h=4);
                translate([16.6,-30.8,deck_z-4]) cylinder(d=5,h=4);
            }
            translate([15,-26,deck_z-5]) cylinder(d=m3_pilot_d,h=6);
        }
        hull() {
            translate([7.0,-34.3,deck_z-6]) cube([1.8,2,7],center=true);
            translate([13,-56,43.5]) cube([1.8,3,5],center=true);
        }
        hull() {
            translate([16.6,-30.8,deck_z-6]) cube([1.8,2,7],center=true);
            translate([47,-56,43.5]) cube([1.8,3,5],center=true);
        }
    }
}

module cup() {
    union() {
        difference() {
            // Rear extension places the open cup directly below every point
            // of the 22-degree outlet; no separate chute is required.
            translate([0,11.5,0]) rounded_box([58,68,29],5);
            translate([0,14,2]) rounded_box([53,62,29],3.5);
        }
        // Wide front pull tab: easy to hook with two fingers, no pinch grip.
        translate([0,-27,10]) rounded_box([32,11,8],3);
        translate([0,-22.5,8]) rounded_box([24,5,9],2);
    }
}

module screen_bezel() {
    difference() {
        cube([76,5,59],center=true);
        cube([screen_active[0]+0.8,8,screen_active[1]+0.8],center=true);
        for(x=[-34,34],z=[-25,25])
            translate([x,0,z]) rotate([90,0,0]) cylinder(d=m3_pilot_d,h=8,center=true);
    }
}

module screen_back_clamp() {
    difference() {
        cube([74,3,57],center=true);
        cube([58,6,40],center=true);
        for(x=[-34,34],z=[-25,25])
            translate([x,0,z]) rotate([90,0,0]) cylinder(d=m3_pilot_d,h=6,center=true);
    }
}

module button_clamp() {
    difference() {
        cube([32,3,21],center=true);
        // The former opening was larger than the 17.62 x 13.42 mm PCB and
        // could not retain it. This window leaves about 1.5 mm bearing on the
        // top, bottom and left edges; the right notch clears the 3-pin plug.
        cube([14.5,6,10.5],center=true);
        translate([11,0,0]) cube([12,6,7],center=true);
        for(x=[-13,13])
            translate([x,0,0]) rotate([90,0,0]) cylinder(d=m3_pilot_d,h=6,center=true);
    }
}

module screen_reference() {
    color([0.05,0.12,0.18,0.8])
        translate([screen_x,-84.5,screen_z])
            cube([screen_board[0],screen_board[2],screen_board[1]],center=true);
}

module button_reference() {
    color("FireBrick") {
        // 3 mm switch body rests against the inside face of the front wall.
        translate([button_x,-base_d/2+wall+1.5,button_z])
            cube([11.5,3,11.5],center=true);
        translate([button_x+button_board_cx,
                   -base_d/2+wall+3+button_board[2]/2,button_z])
            cube(button_board,center=true);
        translate([button_x,-base_d/2-2.5,button_z])
            rotate([90,0,0]) cylinder(d=13,h=5,center=true);
    }
}

module board_reference() {
    color([0.1,0.45,0.2,0.55])
        translate([main_board_x,main_board_y,wall+board_standoff_h+main_board[2]/2])
            cube(main_board,center=true);
}

module accessory_board_references() {
    color([0.15,0.25,0.75,0.65])
        translate([pca_center[0],pca_center[1],wall+board_standoff_h+pca_board[2]/2])
            cube(pca_board,center=true);
    color([0.1,0.35,0.75,0.65])
        translate([rtc_center[0],rtc_center[1],wall+board_standoff_h+rtc_board[2]/2])
            cube(rtc_board,center=true);
}

module assembly() {
    color("LightGray") base();
    color("Gainsboro") translate([0,0,deck_z]) deck();
    color("White") translate([0,0,deck_z+fixed_floor_top-fixed_floor_t]) medication_floor();
    color([0.2,0.75,0.85,0.75]) translate([0,0,rotor_z])
        rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_drive();
    color([0.75,0.9,0.95,0.55]) translate([0,0,rotor_z+cell_h])
        rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_lid();
    color("Orange") translate([servo_axis[0],servo_axis[1],gear_z])
        rotate([0,0,pinion_phase+drive_angle]) pinion();
    color([0.88,0.88,0.9,0.92]) translate([0,0,gear_cover_z]) gear_cover();
    servo_reference();
    as5600_reference();
    color("DimGray") translate([0,0,deck_z+7.7]) encoder_clamp();
    screen_reference();
    button_reference();
    board_reference();
    accessory_board_references();
    color("Gainsboro") translate([screen_x,-base_d/2-2.5,screen_z]) screen_bezel();
    color("DimGray") translate([screen_x,-80.5,screen_z]) screen_back_clamp();
    color("DimGray") translate([button_x,-81.5,button_z]) button_clamp();
    for(x=[main_board_x-65,main_board_x+65],y=[main_board_y-27,main_board_y+27])
        color("DimGray") translate([x,y,8.6]) board_edge_clamp();
    for(x=[rtc_center[0]-12.75,rtc_center[0]+12.75],
        y=[rtc_center[1]-15.85,rtc_center[1]+15.85])
        color("DimGray") translate([x,y,8.6]) rotate([0,0,90]) board_edge_clamp();
    chute();
    color("SlateGray") translate([servo_axis[0],servo_body_cy,deck_z-43]) servo_clamp();
    color("LightSteelBlue") translate([cup_x,-base_d/2+23-cup_pull,7]) cup();
}

module parts_plate() {
    // Review layout: every printable component is spatially separated so a
    // slicer can use "split to objects/parts". This is not an assembled mesh.
    translate([0,0,0]) base();
    translate([230,0,0]) deck();
    translate([230,200,3]) medication_floor();
    translate([75,200,gear_thickness+0.8]) rotor_drive();
    translate([245,200,0]) pinion();
    translate([285,200,0]) cup();
    translate([350,200,-56]) chute();
    translate([400,200,0]) servo_clamp();
    translate([450,200,0]) screen_bezel();
    translate([460,0,0]) gear_cover();
    translate([540,200,2]) rotor_lid();
    translate([600,200,0]) screen_back_clamp();
    translate([660,200,0]) button_clamp();
    translate([700,200,0]) encoder_clamp();
    translate([745,200,0]) board_edge_clamp();
}

module collision_pinion_rotor() {
    intersection() {
        translate([servo_axis[0],servo_axis[1],gear_z]) rotate([0,0,pinion_phase]) pinion();
        translate([0,0,rotor_z]) rotor();
    }
}

module collision_gears() {
    intersection() {
        translate([0,0,gear_z]) rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) large_gear();
        translate([servo_axis[0],servo_axis[1],gear_z])
            rotate([0,0,pinion_phase+drive_angle]) pinion();
    }
}

module collision_gear_outlet() {
    intersection() {
        translate([0,0,gear_z]) large_gear();
        translate([0,0,gear_z-1]) outlet_cut(gear_thickness+3);
    }
}

module collision_gear_deck() {
    intersection() {
        translate([0,0,gear_z]) large_gear();
        translate([0,0,deck_z]) deck();
    }
}

module collision_servo_board() {
    intersection() {
        translate([servo_axis[0],servo_body_cy,
                   gear_z-servo_total_h+servo_body[2]/2])
            cube([servo_body[1],servo_body[0],servo_body[2]],center=true);
        translate([main_board_x,main_board_y,wall+board_standoff_h+main_board[2]/2])
            cube(main_board,center=true);
    }
}

module collision_chute_cup() {
    intersection() {
        chute();
        translate([cup_x,-base_d/2+23-cup_pull,7]) cup();
    }
}

module collision_cup_base() {
    intersection() {
        translate([cup_x,-base_d/2+23-cup_pull,7]) cup();
        base();
    }
}

module collision_rotor_deck() {
    intersection() {
        translate([0,0,rotor_z])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor();
        union() {
            translate([0,0,deck_z]) deck();
            translate([0,0,deck_z+fixed_floor_top-fixed_floor_t]) medication_floor();
        }
    }
}

module collision_floor_drive() {
    intersection() {
        translate([0,0,deck_z+fixed_floor_top-fixed_floor_t]) medication_floor();
        translate([0,0,rotor_z])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_drive();
    }
}

module collision_floor_fasteners() {
    intersection() {
        union() {
            deck();
            translate([0,0,fixed_floor_top-fixed_floor_t]) medication_floor();
        }
        for(a=floor_mount_angles) rotate([0,0,a]) translate([floor_mount_r,0,0]) {
            // Conservative envelope for an M3x8 low-profile pan-head screw.
            translate([0,0,0.25]) cylinder(d=6.0,h=2.0);
            translate([0,0,2.2]) cylinder(d=3.0,h=8.0);
        }
    }
}

module collision_drive_cover() {
    intersection() {
        union() {
            translate([0,0,rotor_z])
                rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor_drive();
            translate([servo_axis[0],servo_axis[1],gear_z])
                rotate([0,0,pinion_phase+drive_angle]) pinion();
        }
        translate([0,0,gear_cover_z]) gear_cover();
    }
}

module collision_chute_base() {
    intersection() {
        chute();
        base();
    }
}

module collision_servo_cradle_board() {
    intersection() {
        translate([servo_axis[0],servo_body_cy,deck_z-43]) servo_clamp();
        translate([main_board_x,main_board_y,wall+board_standoff_h+main_board[2]/2])
            cube(main_board,center=true);
    }
}

module collision_servo_cradle_base() {
    intersection() {
        translate([servo_axis[0],servo_body_cy,deck_z-43]) servo_clamp();
        base();
    }
}

module collision_servo_fasteners_drive() {
    intersection() {
        union() {
            for(x=[-servo_mount_dx/2,servo_mount_dx/2],
                y=[-servo_mount_dy/2,servo_mount_dy/2])
                translate([servo_axis[0]+x,servo_body_cy+y,deck_z-1])
                    cylinder(d=3.0,h=gear_cover_h+5);
        }
        union() {
            translate([0,0,rotor_z]) rotor_drive();
            translate([servo_axis[0],servo_axis[1],gear_z])
                rotate([0,0,pinion_phase]) pinion();
        }
    }
}

module collision_horn_deck() {
    intersection() {
        translate([servo_axis[0],servo_axis[1],gear_z-servo_horn_h])
            cylinder(d=servo_horn_d,h=servo_horn_h);
        translate([0,0,deck_z]) deck();
    }
}

module collision_encoder_clamp_rotor() {
    intersection() {
        translate([0,0,deck_z+7.7]) encoder_clamp();
        translate([0,0,rotor_z])
            rotate([0,0,-drive_angle*pinion_teeth/large_teeth]) rotor();
    }
}

module collision_encoder_clamp_thrust_pads() {
    intersection() {
        translate([0,0,deck_z+7.7]) encoder_clamp();
        translate([0,0,deck_z+deck_t-0.1]) difference() {
            cylinder(r=bearing_tower_ro,h=fixed_floor_top+floor_gap-deck_t);
            translate([0,0,-0.1])
                cylinder(r=bearing_tower_ri,h=fixed_floor_top+floor_gap-deck_t+0.2);
        }
    }
}

if(part=="assembly") assembly();
else if(part=="parts_plate") parts_plate();
else if(part=="base") base();
else if(part=="deck") deck();
else if(part=="medication_floor") medication_floor();
else if(part=="rotor") rotor();
else if(part=="rotor_drive") rotor_drive();
else if(part=="rotor_lid") rotor_lid();
else if(part=="large_gear") large_gear();
else if(part=="pinion") pinion();
else if(part=="cup") cup();
else if(part=="chute") chute();
else if(part=="servo_clamp") servo_clamp();
else if(part=="screen_bezel") screen_bezel();
else if(part=="screen_back_clamp") screen_back_clamp();
else if(part=="button_clamp") button_clamp();
else if(part=="encoder_clamp") encoder_clamp();
else if(part=="board_edge_clamp") board_edge_clamp();
else if(part=="collision_pinion_rotor") collision_pinion_rotor();
else if(part=="collision_gears") collision_gears();
else if(part=="collision_gear_outlet") collision_gear_outlet();
else if(part=="collision_gear_deck") collision_gear_deck();
else if(part=="collision_servo_board") collision_servo_board();
else if(part=="collision_chute_cup") collision_chute_cup();
else if(part=="collision_cup_base") collision_cup_base();
else if(part=="collision_rotor_deck") collision_rotor_deck();
else if(part=="collision_floor_drive") collision_floor_drive();
else if(part=="collision_floor_fasteners") collision_floor_fasteners();
else if(part=="collision_drive_cover") collision_drive_cover();
else if(part=="collision_chute_base") collision_chute_base();
else if(part=="collision_servo_cradle_board") collision_servo_cradle_board();
else if(part=="collision_servo_cradle_base") collision_servo_cradle_base();
else if(part=="collision_servo_fasteners_drive") collision_servo_fasteners_drive();
else if(part=="collision_horn_deck") collision_horn_deck();
else if(part=="collision_encoder_clamp_rotor") collision_encoder_clamp_rotor();
else if(part=="collision_encoder_clamp_thrust_pads") collision_encoder_clamp_thrust_pads();

echo(cells=cells, cell_step=cell_step, gear_ratio=large_teeth/pinion_teeth,
     servo_degrees_per_cell=cell_step*(large_teeth/pinion_teeth),
     gear_center_distance=gear_center_distance);
