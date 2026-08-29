// Separate press-fit U handle with flat sides for support-free side printing.
$fn=96;
pitch=70;
peg_d=5.0;

module rounded_prism(size, r) {
    minkowski() {
        cube([size[0]-2*r,size[1]-2*r,size[2]-2*r],center=true);
        sphere(r=r);
    }
}

module handle_v2() {
    union() {
        // Flat rectangular feet distribute lift force over the 1.2 mm roof.
        for(x=[-pitch/2,pitch/2]) {
            translate([x,0,1]) rounded_prism([14,10,2],0.75);
            // Slight press fit in the shell's 4.8 mm holes.
            translate([x,0,-2.8]) cylinder(d1=4.85,d2=peg_d,h=2.9);
            translate([x,0,11]) rounded_prism([8,8,20],2.5);
        }
        // Fully rounded grip edges; dimensions and 70 mm peg pitch unchanged.
        translate([0,0,18.5]) rounded_prism([78,8,9],3.0);
    }
}

handle_v2();
