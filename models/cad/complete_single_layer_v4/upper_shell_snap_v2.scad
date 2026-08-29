// SmartPill removable upper shell V2: low-force snap feet + press-fit handle.
$fn = 128;

outer = [196,176,36];
corner_r = 10;
wall = 1.2;
roof = 1.2;
hole_x = 85;
hole_y = 75;
handle_pitch = 70;
handle_hole_d = 4.8;

module rounded_box(size,r) {
    hull() for(x=[-size[0]/2+r,size[0]/2-r],
               y=[-size[1]/2+r,size[1]/2-r])
        translate([x,y,0]) cylinder(r=r,h=size[2]);
}

module shell_v2() {
    difference() {
        rounded_box(outer,corner_r);
        // Complete open-bottom cavity; top remains flat and closed except for
        // the two filled press-fit handle holes.
        translate([0,0,-0.1])
            rounded_box([outer[0]-2*wall,outer[1]-2*wall,
                         outer[2]-roof+0.1],corner_r-wall);

        // Four shallow internal detents. They do not penetrate the side wall.
        for(sx=[-1,1],sy=[-1,1])
            translate([sx*(outer[0]/2-wall+0.15),sy*hole_y,8.0])
                cube([0.50,9,4.0],center=true);

        // Handle pegs fill these holes after assembly; no screws are visible.
        for(x=[-handle_pitch/2,handle_pitch/2])
            translate([x,0,outer[2]-roof-0.1])
                cylinder(d=handle_hole_d,h=roof+0.2);
    }
}

shell_v2();
