// MT6701-to-Deck adapter.
// Module hole spacing: 17 x 17 mm. Existing Deck hole spacing: 28 x 28 mm.
$fn = 64;
m2_clearance_d = 2.2;
adapter_size = 38;
adapter_t = 6;
module_half = 17/2;
deck_half = 28/2;

difference() {
    // Rounded square frame fits inside the Deck's annular guide rail.
    linear_extrude(adapter_t) offset(r=2) square([34,34],center=true);

    // Large square-like component opening. Only the four corners are clipped
    // to retain material around the 17 x 17 mm M2 clearance holes.
    translate([0,0,-0.5]) linear_extrude(adapter_t+1)
        polygon([[-10,-4],[-4,-10],[4,-10],[10,-4],
                 [10,4],[4,10],[-4,10],[-10,4]]);
    translate([-4,10,-0.5]) cube([8,9,adapter_t+1]);

    // M2 clearance holes: the four MT6701 screws pass through freely.
    for(x=[-module_half,module_half],y=[-module_half,module_half])
        translate([x,y,-0.5]) cylinder(d=m2_clearance_d,h=adapter_t+1);

    // M2 clearance holes: screws pass through and bite into the Deck.
    for(x=[-deck_half,deck_half],y=[-deck_half,deck_half])
        translate([x,y,-0.5]) cylinder(d=m2_clearance_d,h=adapter_t+1);
}
