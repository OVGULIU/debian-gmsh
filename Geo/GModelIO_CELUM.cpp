// Gmsh - Copyright (C) 1997-2013 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@geuz.org>.

#include "GModel.h"
#include "OS.h"
#include "MTriangle.h"
#include "MQuadrangle.h"

class CelumInfo{
public:
  SVector3 normal, dirMax, dirMin;
  double curvMax, curvMin;
};

int GModel::writeCELUM(const std::string &name, bool saveAll,
                       double scalingFactor)
{
  std::string namef = name + "_f";
  FILE *fpf = Fopen(namef.c_str(), "w");
  if(!fpf){
    Msg::Error("Unable to open file '%s'", namef.c_str());
    return 0;
  }

  std::string names = name + "_s";
  FILE *fps = Fopen(names.c_str(), "w");
  if(!fps){
    Msg::Error("Unable to open file '%s'", names.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  // count faces and vertices; the CELUM format duplicates vertices on the
  // boundary of CAD patches
  int numf = 0, nums = 0;
  for(fiter it = firstFace(); it != lastFace(); it++){
    GFace *f = *it;
    if(!saveAll && f->physicals.empty()) continue;
    numf += f->triangles.size();
    std::set<MVertex*> vset;
    for(unsigned int i = 0; i < f->triangles.size(); i++){
      for(int j = 0; j < 3; j++)
        vset.insert(f->triangles[i]->getVertex(j));
    }
    nums += vset.size();
  }

  Msg::Info("Writing %d triangles and %d vertices", numf, nums);

  int idf = 1, ids = 1;
  /*
   * un fichier de facettes
     - nombre de facettes
     - ligne vide
     ... pour chaque facette
     - num�ro de la facette (commence � 1 : utilis� dans les erreurs de lectures)
     - cha�ne de caract�res (nom de la partie g�om�trique, mat�riau,... )
     - la liste des 3 num�ros de sommets
     - ligne vide
     ...
   * un fichier de sommets
     - nombre de sommets
     - facteur de conversion
     - ligne vide
     ... pour chaque sommet
     - num�ro du sommet (commence � 1 : utilis� dans les erreurs de lectures)
     - coordonn�es cart�siennes du sommet
     - composantes de la normale ext�rieure orient�e
     - valeur de la 1i�re courbure principale
     - valeur de la 2i�me courbure principale
     - composantes de la 1i�re direction principale
     - composantes de la 2i�me direction principale
     - ligne blanche
     ...
  */
  fprintf(fpf, "%d\n\n", numf);
  fprintf(fps, "%d %g\n\n", nums, 1.0);
  for(fiter it = firstFace(); it != lastFace(); it++){
    GFace *f = *it;
    if(!saveAll && f->physicals.empty()) continue;
    std::vector<MVertex*> vvec;
    std::map<MVertex*, CelumInfo> vmap;
    for(unsigned int i = 0; i < f->triangles.size(); i++){
      fprintf(fpf, "%d \"face %d\"", idf++, f->tag());
      for(int j = 0; j < 3; j++){
        MVertex *v = f->triangles[i]->getVertex(j);
        if(!vmap.count(v)){
          v->setIndex(ids++);
          SPoint2 param;
          bool ok = reparamMeshVertexOnFace(v, f, param);
          if(!ok)
            Msg::Warning("Could not reparamtrize vertex %d on face %d",
                         v->getNum(), f->tag());
          CelumInfo info;
          info.normal = f->normal(param);
          f->curvatures(param, &info.dirMax, &info.dirMin,
                        &info.curvMax, &info.curvMin);
          vmap[v] = info;
          vvec.push_back(v);
        }
        fprintf(fpf, " %d", v->getIndex());
      }
      fprintf(fpf, "\n\n");
    }
    for(unsigned int i = 0; i < vvec.size(); i++){
      MVertex *v = vvec[i];
      std::map<MVertex *, CelumInfo>::iterator it = vmap.find(v);
      fprintf(fps, "%d %g %g %g %g %g %g %g %g %g %g %g %g %g %g\n\n",
              it->first->getIndex(), it->first->x(), it->first->y(), it->first->z(),
              it->second.normal.x(), it->second.normal.y(),
              it->second.normal.z(), it->second.curvMin, it->second.curvMax,
              it->second.dirMin.x(), it->second.dirMin.y(), it->second.dirMin.z(),
              it->second.dirMax.x(), it->second.dirMax.y(), it->second.dirMax.z());
    }
  }

  fclose(fpf);
  fclose(fps);
  return 1;
}
