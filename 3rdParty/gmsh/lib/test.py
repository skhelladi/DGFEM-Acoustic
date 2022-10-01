
import gmsh

gmsh.initialize()

order = 3

gmsh.model.add("my test model");
gmsh.model.occ.addBox(0,0,0, 1,1,1);
gmsh.model.occ.synchronize()
gmsh.option.setNumber("Mesh.MeshSizeMin", 0.025)
gmsh.option.setNumber("Mesh.MeshSizeMax", 0.025)
gmsh.model.mesh.generate(3)
gmsh.model.mesh.setOrder(order)

faceNumNodes = int((order + 1) * (order + 2) / 2)
                                                
# get tets and faces
elementType = gmsh.model.mesh.getElementType("tetrahedron", order)
face_type = 3
tets, _ = gmsh.model.mesh.getElementsByType(elementType)
faces = gmsh.model.mesh.getElementFaceNodes(elementType, face_type)

print(len(tets))
print(len(faces))

# compute face x tet incidence
print("compute face x tet incidence")
fxt = {}
for i in range(0, len(faces), faceNumNodes):
    #print(i+1," - faces[i:i+3]",faces[i:i+faceNumNodes])
    f = tuple(sorted(faces[i:i+faceNumNodes]))
    #print("f",f)
    t = tets[int(i/(4*faceNumNodes))]
    #print("-> t:",t)
    if not f in fxt:
        fxt[f] = [t]
        #print("f",f)
    else:
        fxt[f].append(t)
        #print("t:",t)
   # print("fxt[f]",fxt[f])    

print("fxt",fxt)
# compute neighbors by face
print("compute neighbors by face")
txt = {}
for i in range(0, len(faces), faceNumNodes):
    f = tuple(sorted(faces[i:i+faceNumNodes]))
    t = tets[int(i/(4*faceNumNodes))]
    if not t in txt:
        txt[t] = set()
    for tt in fxt[f]:
        if tt != t:
            txt[t].add(tt)

print("neighbors by face: ", txt)

gmsh.finalize()