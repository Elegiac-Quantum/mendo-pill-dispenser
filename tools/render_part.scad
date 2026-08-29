// Headless OpenSCAD wrapper used to generate accurate README previews.
// Pass part_path and part_color with -D on the command line.

$fn = 64;
color(part_color)
    import(part_path, convexity = 12);
