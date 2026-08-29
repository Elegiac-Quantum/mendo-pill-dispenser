// Functional button mounting coupon: wall section + screw-on U clamp.
// Measured PCB: 20 x 13 mm; button centre 7 mm from left edge.
$fn = 64;
part = is_undef(part) ? "print_plate" : part;

wall_t = 2.4;
coupon = [40,30];
button_d = 13.6;
board = [20.0,13.0,1.6];
fit = 0.35;
board_cx = board[0]/2-7.0; // +3 mm relative to button centre
guide_t = 1.2;
switch_body = [11.5,11.5,3.0];
moving_neck_h = 4.5;
cap_d = 13.0;
cap_h = 5.0;
seat_h = switch_body[2]+board[2]-0.1; // 4.5 mm; 0.1 mm clamp preload
m2_pilot = 1.7;
m2_clear = 2.3;
screw_x_left = -13.0;
screw_x_right = 16.5;
screw_xs = [screw_x_left,screw_x_right];
screw_y = 9.5;
boss_d = 6.0;
clamp_t = 6.0;

left = board_cx-(board[0]+fit)/2;
right = board_cx+(board[0]+fit)/2;
bottom = -(board[1]+fit)/2;
top = (board[1]+fit)/2;

module wall_coupon() {
    difference() {
        union() {
            difference() {
                translate([0,0,wall_t/2]) cube([coupon[0],coupon[1],wall_t],center=true);
                translate([0,0,-0.1]) cylinder(d=button_d,h=wall_t+0.2);
            }
            // Three locating edges; the right side is open for the 3-pin plug.
            translate([left-guide_t/2,0,wall_t+seat_h/2])
                cube([guide_t,board[1]+fit+2*guide_t,seat_h],center=true);
            for(y=[bottom-guide_t/2,top+guide_t/2])
                translate([(left+right)/2,y,wall_t+seat_h/2])
                    cube([right-left,guide_t,seat_h],center=true);
            for(x=screw_xs,y=[-screw_y,screw_y])
                translate([x,y,wall_t]) cylinder(d=boss_d,h=seat_h);
        }
        for(x=screw_xs,y=[-screw_y,screw_y])
            // Blind thread paths; use M2 x 10 mm with the 6 mm clamp.
            translate([x,y,0.8]) cylinder(d=m2_pilot,h=wall_t+seat_h-0.6);
    }
}

module back_clamp() {
    bridge_x = board_cx; // user-validated clear centre band, away from solder joints
    bridge_w = 7.0;      // includes 2 mm extra support toward the M2 holes
    bridge_cx = bridge_x-1.0; // keep the far edge fixed; extend only hole-ward
    difference() {
        union() {
            // Centre bridge avoids the solder joints near the former left edge.
            translate([bridge_cx,0,clamp_t/2])
                cube([bridge_w,board[1]+2.2,clamp_t],center=true);
            for(y=[-6.2,6.2])
                translate([(left+right)/2-0.4,y,clamp_t/2])
                    cube([right-left+0.8,2.2,clamp_t],center=true);
            // Existing left pair connects to the strengthened centre bridge.
            for(y=[-screw_y,screw_y])
                hull() {
                    translate([screw_x_left,y,0]) cylinder(d=boss_d,h=clamp_t);
                    translate([bridge_cx,y*0.66,clamp_t/2])
                        cube([bridge_w,2.4,clamp_t],center=true);
                }
            // New right pair joins the top/bottom rails while leaving the
            // central 3-pin connector path completely open.
            for(y=[-screw_y,screw_y])
                hull() {
                    translate([screw_x_right,y,0]) cylinder(d=boss_d,h=clamp_t);
                    translate([right-0.4,y*0.66,clamp_t/2])
                        cube([2.4,2.4,clamp_t],center=true);
                }
        }
        for(x=screw_xs,y=[-screw_y,screw_y])
            translate([x,y,-0.1]) cylinder(d=m2_clear,h=clamp_t+0.2);
    }
}

module print_plate() {
    translate([-23,0,0]) wall_coupon();
    translate([22,0,0]) back_clamp();
}

module assembly_preview() {
    wall_coupon();
    // Conservative reference for the measured button stack.
    color("Orange",0.8)
        translate([0,0,wall_t+switch_body[2]/2])
            cube(switch_body,center=true);
    color("Orange",0.8)
        translate([0,0,wall_t-moving_neck_h/2])
            cylinder(d=9.0,h=moving_neck_h,center=true);
    color("Red",0.8)
        translate([0,0,wall_t-moving_neck_h-cap_h/2])
            cylinder(d=cap_d,h=cap_h,center=true);
    color("FireBrick",0.7)
        translate([board_cx,0,wall_t+switch_body[2]+board[2]/2])
            cube(board,center=true);
    color("DimGray") translate([0,0,wall_t+seat_h]) back_clamp();
}

if(part == "wall") wall_coupon();
else if(part == "clamp") back_clamp();
else if(part == "assembly") assembly_preview();
else print_plate();
