// Gmsh project created on Wed Nov 29 09:52:42 2023
SetFactory("OpenCASCADE");
//+
Point(1) = {-300, 0, 0, 1.0};
Point(2) = {-300, 300, 0, 1.0};
Point(3) = {3000, 0, 0, 1.0};
Point(4) = {3000, 300, 0, 1.0};
//+
Show "*";
//+
Point(5) = {0, 0, 0, 1.0};
//+
Point(6) = {-200, 0, 0, 1.0};
//+
Point(7) = {200, 0, 0, 1.0};
//+

//+
Line(1) = {1,2};
Line(2) = {2,4};
Line(3) = {4,3};
Line(4) = {3,7};
Line(5) = {1,6};

Circle(6) = {6, 5, 7};

Curve Loop(1) = {1,2,3,4,5,6};

Plane Surface(1) = {1}; 

Physical Curve("abs", 1) = {1,2,3};
//+
Physical Curve("ref", 2) = {4,5,6};

//+
Show "*";
//+
Hide {
  Point{5}; Surface{1}; 
}
