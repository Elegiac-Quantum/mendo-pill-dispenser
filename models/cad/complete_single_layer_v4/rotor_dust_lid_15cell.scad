// Dust lid for the physically tested 15-cell rotor (OD 134, wall 1.8 mm).
// Two support-free parts: lid body and press-in pull handle.
$fn = 128;
part = is_undef(part) ? "assembly" : part;

rotor_outer_r = 67.0;
rotor_inner_r = 65.2;
lid_r = 67.8;
lid_t = 2.0;
ring_outer_r = 64.85;       // 0.35 mm radial clearance from rotor inner wall
ring_inner_r = 63.25;
ring_h = 3.5;
handle_hole_d = 5.0;

module lid_body() {
    difference() {
        union() {
            cylinder(r=lid_r,h=lid_t);
            // This ring prints upward; flip the finished lid for installation.
            translate([0,0,lid_t-0.15])
                difference() {
                    cylinder(r=ring_outer_r,h=ring_h);
                    translate([0,0,-0.1]) cylinder(r=ring_inner_r,h=ring_h+0.2);
                }
            // Six tiny friction pads provide a light, distributed snap fit.
            for(a=[0:60:300]) rotate([0,0,a])
                translate([64.8,-2.1,lid_t+0.65])
                    cube([0.35,4.2,1.8]);
        }
        translate([0,0,-0.1]) cylinder(d=handle_hole_d,h=lid_t+0.2);
    }
}

module pull_handle() {
    // Print the broad grip face on the bed. Flip before inserting into lid.
    union() {
        cylinder(d=22,h=3);
        translate([0,0,2.9]) cylinder(d1=18,d2=11,h=3.2);
        translate([0,0,6.0]) cylinder(d=8,h=3.1);
        // 5.15 mm peg gives 0.15 mm diametral interference in the 5 mm hole.
        translate([0,0,9.0]) cylinder(d=5.15,h=2.8);
    }
}

module assembly_preview() {
    color([0.72,0.88,0.96,0.75]) lid_body();
    color("Orange") translate([0,0,13.8]) rotate([180,0,0]) pull_handle();
}

module one_piece_lid() {
    // Installed orientation: central locating boss down, pull handle up.
    // The rotor centre is recessed 13.5 mm (cell top 20, centre disk 6.5).
    union() {
        // 69.0 mm boss fits the 70.0 mm central recess with 0.5 mm radial
        // clearance; 8 mm depth leaves 5.5 mm above the centre disk.
        cylinder(d=69.0,h=8.0);
        // Six shallow pads reduce rattle without creating a hard snap fit.
        for(a=[0:60:300]) rotate([0,0,a])
            translate([34.45,-2.0,2.0]) cube([0.15,4.0,4.0]);
        translate([0,0,8.0]) cylinder(r=lid_r,h=lid_t);
        translate([0,0,9.8]) cylinder(d=8,h=3.2);
        translate([0,0,12.8]) cylinder(d1=11,d2=18,h=3.2);
        translate([0,0,15.8]) cylinder(d=22,h=3.0);
    }
}

// Support-free replacement for one_piece_lid(). The lid is exported in its
// print orientation (flat cover face on the bed); flip it for installation.
center_boss_d = 69.0;
center_boss_h = 8.0;
thread_pitch = 3.0;
thread_length = 9.0;
thread_core_r = 5.2;
thread_major_r = 6.0;
thread_clearance = 0.50;
thread_steps_per_turn = 28;

// Rounded coarse thread is more tolerant of FDM layer error than a sharp
// metric profile. Female cutter uses extra radial and axial clearance.
module rounded_helix(r=5.6,length=9,pitch=3,ball_r=0.4,z_scale=1.5) {
    turns = length/pitch;
    steps = floor(turns*thread_steps_per_turn);
    for(i=[0:steps-1]) hull() {
        for(j=[i,i+1]) {
            a = 360*j/thread_steps_per_turn;
            translate([r*cos(a),r*sin(a),pitch*j/thread_steps_per_turn])
                scale([1,1,z_scale]) sphere(r=ball_r,$fn=20);
        }
    }
}

module male_coarse_thread() {
    intersection() {
        union() {
            cylinder(r=thread_core_r,h=thread_length);
            rounded_helix(r=(thread_core_r+thread_major_r)/2,
                          length=thread_length,pitch=thread_pitch);
        }
        // Full diameter at the shoulder, tapered lead at the insertion tip.
        union() {
            cylinder(r=thread_major_r+0.1,h=thread_length-1.2);
            translate([0,0,thread_length-1.2])
                cylinder(r1=thread_major_r+0.1,
                         r2=thread_core_r-0.05,h=1.2);
        }
    }
}

module female_thread_cutter() {
    union() {
        cylinder(r=thread_core_r+thread_clearance,
                 h=thread_length+0.3);
        rounded_helix(r=(thread_core_r+thread_major_r)/2+thread_clearance,
                      length=thread_length+0.15,pitch=thread_pitch,
                      ball_r=0.7,z_scale=1.7);
        // Wide 1.2 mm lead-in at the visible cover face.
        cylinder(r1=thread_major_r+thread_clearance+0.7,
                 r2=thread_core_r+thread_clearance,h=1.2);
    }
}

module center_locating_lid_body() {
    difference() {
        union() {
            cylinder(r=lid_r,h=lid_t);
            // Becomes the downward locating boss after installation.
            translate([0,0,lid_t-0.2])
                cylinder(d=center_boss_d,h=center_boss_h);
            for(a=[0:60:300]) rotate([0,0,a])
                translate([center_boss_d/2-0.05,-2.0,lid_t+1.8])
                    cube([0.15,4.0,4.0]);
        }
        // Through coarse female thread; open end avoids any bridged ceiling.
        translate([0,0,-0.1]) female_thread_cutter();
    }
}

module center_locating_pull_handle() {
    // Print the 22 mm grip face on the bed, then flip and screw into the lid.
    union() {
        cylinder(d=22,h=3);
        translate([0,0,2.9]) cylinder(d1=18,d2=14,h=3.2);
        translate([0,0,6.0]) cylinder(d=14,h=3.1);
        translate([0,0,9.0]) male_coarse_thread();
    }
}

module center_two_part_preview() {
    // Installed orientation: locating boss downward, grip upward.
    body_h = lid_t-0.2+center_boss_h;
    color([0.72,0.88,0.96,0.75])
        translate([0,0,body_h]) rotate([180,0,0])
            center_locating_lid_body();
    color("Orange")
        translate([0,0,body_h+thread_length+0.1]) rotate([180,0,0])
            center_locating_pull_handle();
}

if(part=="lid") lid_body();
else if(part=="handle") pull_handle();
else if(part=="one_piece") one_piece_lid();
else if(part=="center_lid") center_locating_lid_body();
else if(part=="center_handle") center_locating_pull_handle();
else if(part=="center_two_part") center_two_part_preview();
else assembly_preview();
