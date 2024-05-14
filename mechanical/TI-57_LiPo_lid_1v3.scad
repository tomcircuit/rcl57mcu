/*
  TI-57 Battery Lid for LiPo Pack V1.3
  This is intended to hold a PCB and a LiPo battery for RCL-57.

  Dimensions measured from a TI-57 and a TI-55 calculator, and several BP-7 packs.
  
  1.0 - high-walls - awful latch geometry
  1.1 - high-walls - still bad latch geometery
  1.2 - ref'd off of thingiverse "Majestic_line_battery_cover.stl" file - better...
  1.3 - increased latch tab Z, decreased latch spring wall thickness - much better!

  T. LeMense
  May 13, 2024
*/


$fa = 1;
$fs = 0.4;

// W is width, and is along X AXIS
// L is length, and is along Y AXIS
// H is height, and is along Z AXIS
// T is thickness, and is along either X or Y AXIS 

// baseplate (first surface)
base_w = 63;  
base_l = 50.5;   
base_h = 1.4;  

/*
// Majestic Line Battery Cover reference STL
translate([base_w/2,base_l/2,0])
rotate([0,0,270])
import("C:/Users/lemenset/Personal/TI57_upgrade/mechanical/Majestic_line_Battery_cover.stl");
//*/

// "ledges" are setbacks around perimeter of base
ledge_long_edge = 2.2;  // ledge along long sides (estimated)
ledge_short_edge = 1.7; // ledge step-back along non-latch short end
latch_setback = 3.5;  // setback of latch (measured)
ledge_latch_edge = 4.5;  // setback of latch-side wall (measured)

// "superstructure" box - around entire cavity
super_w = base_w - ledge_latch_edge + ledge_short_edge; // width (long edge)
super_l = base_l - ledge_long_edge - ledge_long_edge;  // length (short edge)
super_h = 2;        // height of superstructure box
super_wall_t = 1.75;     // superstructure wall thickness
super_floor_add_h = 0.5; // additional floor thickness

echo("superstructure inside width is", super_w-2*super_wall_t);
echo("superstructure inside length is", super_l-2*super_wall_t);

// PCB information
pcb_l = 25;
pcb_w = 20;
pcb_t = 1.6;
pcb_hole_from_edge = 3;

// PCB position w.r.t. lid
pcb_x = 0;
pcb_y = ledge_long_edge + super_wall_t;
pcb_comp_z = 6; 
pcb_solder_z = pcb_comp_z-pcb_t-base_h;

// latch 'tab' is an extruded 2D polygon
latch_tab_w1 = 2.5; // "thickest" part of latch feature
latch_tab_h = 2.5;  // "height" of latch tab
latch_tab_w2 = 1;   // "thinnest" part of latch tab
latch_tab_l = 16.0; // "length" of latch tab
latch_tab_z = 5.5;   // relative to base_h

// construct latch tab polygon point set
p0 = [0, 0];
p1 = [0, latch_tab_h];
p2 = [latch_tab_w2,latch_tab_h];
p3 = [latch_tab_w1,0];
tab_points = [p0, p1, p2, p3];

// hinge cutout is extruded 2D polygon
cutout_l = 10.0;
cutout_h = latch_tab_z;

// built hinge cutout polygon point set
p4 = [0, 0];
p5 = [0, cutout_h];
p6 = [(latch_tab_l - cutout_l)/2,cutout_h];
p7 = [latch_tab_w2,0];
cutout_points = [p4, p5, p6, p7];

module RoundedBoss(side,height,hole_dia){
    difference() {
        translate([0,0,0])
        cylinder(d=side,h=height);
        
        translate([0,0,height/3])
        cylinder(d=hole_dia,h=height/1.5);
    }
}

function RoundedBossHoleCenter(side,height,hole_dia) = [side/2,side/2,0];   

//location of the PCB mounting bosses
boss_l = 4.5;
boss_h_dia = 2.2;
boss_inset = (boss_l - boss_h_dia)/4;
h1 = [pcb_x+pcb_hole_from_edge,pcb_y+pcb_w-pcb_hole_from_edge,0];
h2 = h1 + [pcb_l-2*pcb_hole_from_edge,-pcb_w+2*pcb_hole_from_edge,0];

// build the lid using additive and subtractive items
union(){
    difference() {
        union() {        
        // additive items        
            
        // the baseplate
        cube(size=[base_w, base_l, base_h]);
            
        // the superstructure volume
        translate([-ledge_short_edge, ledge_long_edge, base_h])
            cube([super_w, super_l,super_h]); 
        
        // the PCB box volume
        translate([-ledge_short_edge, ledge_long_edge, base_h])
            cube([super_wall_t+pcb_l,super_wall_t+pcb_w,pcb_comp_z-base_h]);
    
        // the locking tab
        translate([base_w - latch_setback,ledge_long_edge+(super_l/2)+(latch_tab_l/2),latch_tab_z+base_h]) 
            rotate([90,0,0]) 
                linear_extrude(height=latch_tab_l) polygon(tab_points);
    
        // the locking tab spring wall
        translate([base_w - latch_setback,ledge_long_edge+(super_l/3),base_h])
            cube([1, super_l/3, latch_tab_z]);
        
        // the locking tab "backstop"
        translate([base_w - ledge_latch_edge-super_wall_t,ledge_long_edge+(super_l/3),base_h])
            cube([super_wall_t,super_l/3, latch_tab_z+latch_tab_h]);
    
        } //union
        
        // subtractive items                    
        // remove tab release slot from base
        translate([base_w - 1.5,(base_l/2)-((latch_tab_l-2)/2),-0.25]) 
            cube([2.5,(latch_tab_l-2),2]);
            
        // remove the two hinge cutouts
        translate([base_w-ledge_latch_edge,ledge_long_edge+(super_l/2)-(latch_tab_l/2)-0.15,base_h+3])
            rotate([90,0,90])
                linear_extrude(height=3*super_wall_t)
                    polygon(cutout_points);
        
        translate([super_w+ledge_latch_edge,ledge_long_edge+(super_l/2)+(latch_tab_l/2)+0.15,base_h+3])
            rotate([90,0,-90]) 
                linear_extrude(height=3*super_wall_t) 
                    polygon(cutout_points);        
    
        // flatten the rounded wall edges
        translate([-2, 1.25+super_wall_t, base_h+12.5])
            cube([5,super_l-2*super_wall_t,5]);
        translate([base_w-4, 1.25+super_wall_t, base_h+10])
            cube([5,super_l-2*super_wall_t,5]);
    
        // remove the superstructure insides
        translate([super_wall_t-ledge_short_edge, ledge_long_edge+super_wall_t, base_h+super_floor_add_h])
            cube([super_w-2*super_wall_t, super_l-2*super_wall_t, 20]); 
        
        // etch some text into the floor
        translate([2*base_w/3,base_l/3,base_h])
            rotate([0,0,180])
                linear_extrude(3)
                    text( "v1.3", size= 8, ,halign = "center", valign = "bottom" );
    } //difference
    // objects located inside the superstructure cavity
    
    // PCB mounting screw bosses   
    translate(h1 + [0,0,base_h+super_floor_add_h])
        RoundedBoss(boss_l,pcb_solder_z,boss_h_dia);  // for #2 screw

    translate(h2 + [0,0,base_h+super_floor_add_h])
        RoundedBoss(boss_l,pcb_solder_z,boss_h_dia);  // for #2 screw    

} //union

/*
// uncomment this block if a pcb "model" is desired
pcb_model_z = pcb_solder_z+base_h;
hole_dia = 0.086 * 25.4;

translate([0,0,pcb_model_z]) 
    color("green") 
    linear_extrude(height=pcb_t) 
    difference(){
       translate ([pcb_x,pcb_y,0]) square ([pcb_l,pcb_w],false); 
       translate(h1) circle(d = hole_dia);
       translate(h2) circle(d = hole_dia);
    }
//*/



