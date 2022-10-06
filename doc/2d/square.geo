//+
SetFactory("OpenCASCADE");
Rectangle(1) = {-5, -5, 0, 10, 10, 0};
//+
Rotate {{0, 0, 1}, {0, 0, 0}, Pi/4} {
  Curve{3}; Curve{4}; Curve{1}; Curve{2}; 
}

//+
//Physical Curve("Absorbing") = {4, 1, 2, 3};
//+
//Physical Surface("Domain") = {1};

//+
Hide "*";
//+
Show {
  Point{3}; Point{4}; Point{5}; Point{6}; Curve{1}; Curve{2}; Curve{3}; Curve{4}; 
}
//+
Curve Loop(2) = {3, 4, 1, 2};
//+
Plane Surface(2) = {2};
//+
Physical Curve("Absorbing", 9) = {4, 3, 2, 1};
//+
Physical Surface("Domain", 10) = {2};
