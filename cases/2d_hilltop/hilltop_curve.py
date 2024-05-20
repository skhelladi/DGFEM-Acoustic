# this function provide the curve of a hill
# h(x) = h_max*cos(pi()*x/2/l)^2 for -l<x<l

import numpy as np
import matplotlib.pyplot as plt

def hilltop_curve(x, l, h_max):
    return h_max*np.cos(np.pi*x/2/l)**2

def hilltop_curve_plot(l, h_max, overall_length=3000):
    x = np.linspace(-l, l, 100)
    y = hilltop_curve(x, l, h_max)
    plt.plot(x, y)
    x_grouned = np.linspace(l, overall_length, 2)
    y_grouned = np.zeros(2)
    plt.plot(x_grouned, y_grouned, 'k')
     

    # save the coordinates of the hilltop curve in a file (txt format)
    file_name = 'hilltop_curve.txt'
    separator = ';'
    with open(file_name, 'w') as f:
       # f.write('x'+separator+'y'+separator+'z\n')
        f.write(str(x[0])+separator+str(overall_length/10)+ separator+ str(0)+ '\n')
        for i in range(len(x)):
            f.write(str(x[i]) + separator + str(y[i]) + separator+ str(0)+'\n')
        for i in range(len(x_grouned)):
            f.write(str(x_grouned[i]) + separator + str(y_grouned[i]) + separator+ str(0)+ '\n')
        f.write(str(x_grouned[-1])+separator+str(overall_length/10)+ separator+ str(0))

    # save the coordinates of the hilltop curve in a file (ply format)
    file_name = 'hilltop_curve.ply'
    with open(file_name, 'w') as f:
        f.write('ply\n')
        f.write('format ascii 1.0\n')
        f.write('element vertex '+str(len(x)+len(x_grouned)+2)+'\n')
        f.write('property float x\n')
        f.write('property float y\n')
        f.write('property float z\n')
        f.write('end_header\n')
        f.write(str(x[0])+' '+str(overall_length/10)+' '+str(0)+'\n')
        for i in range(len(x)):
            f.write(str(x[i]) + ' ' + str(y[i]) + ' ' + str(0) + '\n')
        for i in range(len(x_grouned)):
            f.write(str(x_grouned[i]) + ' ' + str(y_grouned[i]) + ' ' + str(0) + '\n')
        f.write(str(x_grouned[-1])+' '+str(overall_length/10)+' '+str(0)+'\n')
     
     # save in a gmsh geo file, the orrder of the points should be clockwise
    file_name = 'hilltop_curve.geo'
    with open(file_name, 'w') as f:
        f.write('Point(1) = {'+str(x[0])+', '+str(overall_length/10)+', '+str(0)+'};\n')
        for i in range(len(x)-1):
            f.write('Point('+str(i+2)+') = {'+str(x[i])+', '+str(y[i])+', '+str(0)+'};\n')
        for i in range(len(x_grouned)):
            f.write('Point('+str(i+len(x)+1)+') = {'+str(x_grouned[i])+', '+str(y_grouned[i])+', '+str(0)+'};\n')
        f.write('Point('+str(len(x)+len(x_grouned)+1)+') = {'+str(x_grouned[-1])+', '+str(overall_length/10)+', '+str(0)+'};\n')
        f.write('Line(1) = {1, 2};\n')
        for i in range(len(x)-1):
            f.write('Line('+str(i+2)+') = {'+str(i+2)+', '+str(i+3)+'};\n')
        for i in range(len(x_grouned)-1):
            f.write('Line('+str(i+len(x)+1)+') = {'+str(i+len(x)+1)+', '+str(i+len(x)+2)+'};\n')
        f.write('Line('+str(len(x)+len(x_grouned))+') = {'+str(len(x)+len(x_grouned))+', '+str(len(x)+len(x_grouned)+1)+'};\n')
        # line from the last point to the first point
        f.write('Line('+str(len(x)+len(x_grouned)+1)+') = {'+str(len(x)+len(x_grouned)+1)+', '+str(1)+'};\n')
        # add the line loop respectin the clockwise order
        f.write('Line Loop(1) = {1')
        for i in range(len(x)-1):
            f.write(', '+str(i+2))
        for i in range(len(x_grouned)+1):
            f.write(', '+str(i+len(x)+1))
        f.write('};\n')
        # add the physical curves : ground and hilltop are "ref", the others are "abs"
        f.write('Physical Curve("abs",1) = {1, 102, 103};\n')
        f.write('Physical Curve("ref",2) = {2');
        for i in range(len(x)-1):
            f.write(', '+str(i+3))
        f.write('};\n')
        


        
        

        
       
        # add the surface
        f.write('Plane Surface(1) = {1};\n')
    
    # file_name = 'hilltop_curve.geo'
    # with open(file_name, 'w') as f:
    #     for i in range(len(x)):
    #         f.write('Point('+str(i+2)+') = {'+str(x[i])+', '+str(y[i])+', '+str(0)+'};\n')
    #     for i in range(len(x_grouned)):
    #         f.write('Point('+str(i+len(x)+2)+') = {'+str(x_grouned[i])+', '+str(y_grouned[i])+', '+str(0)+'};\n')
    #     f.write('Point('+str(len(x)+len(x_grouned)+2)+') = {'+str(x_grouned[-1])+', '+str(overall_length/10)+', '+str(0)+'};\n')
    #     f.write('Line(1) = {1, 2};\n')
    #     for i in range(len(x)-1):
    #         f.write('Line('+str(i+2)+') = {'+str(i+2)+', '+str(i+3)+'};\n')
    #     for i in range(len(x_grouned)-1):
    #         f.write('Line('+str(i+len(x)+2)+') = {'+str(i+len(x)+2)+', '+str(i+len(x)+3)+'};\n')
    #     f.write('Line('+str(len(x)+len(x_grouned)+1)+') = {'+str(len(x)+len(x_grouned)+1)+', '+str(len(x)+len(x_grouned)+2)+'};\n')
    #     # line from the last point to the first point
    #     f.write('Line('+str(len(x)+len(x_grouned)+2)+') = {'+str(len(x)+len(x_grouned)+2)+', '+str(1)+'};\n')
    #     f.write('Line Loop(1) = {1')
    #     for i in range(len(x)-1):
    #         f.write(', '+str(i+2))
    #     for i in range(len(x_grouned)):
    #         f.write(', '+str(i+len(x)+2))
    #     f.write(', '+str(len(x)+len(x_grouned)+2)+'};\n')
    #     f.write('Point(1) = {'+str(x[0])+', '+str(overall_length/10)+', '+str(0)+'};\n')
    #     f.write('Plane Surface(1) = {1};\n')
    # x and y have the same unit
    # plt.xlabel('x (m)')
    # plt.ylabel('y (m)')
    # plt.title('Hilltop Curve')
    # # x and y axis have the same scale
    # # plt.axis('equal')
    # plt.ylim(0,300)
    # plt.show()

if __name__ == '__main__':
   hilltop_curve_plot(260, 100)
  

    