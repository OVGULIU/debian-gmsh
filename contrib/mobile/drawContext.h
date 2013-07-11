#ifndef _DRAW_GMODEL_H_
#define _DRAW_GMODEL_H_

#if !defined(BUILD_ANDROID)
#define BUILD_IOS 1
#endif

#include <string>

#if defined(BUILD_IOS)
#include <Gmsh/PView.h>
#include <Gmsh/PViewOptions.h>
#include <Gmsh/Context.h>
#endif

#include "movePosition.h"

int onelab_cb(std::string);

class drawContext{
private:
	float _translate[3], _scale[3]; // current translation and scale
	double _rotate[16]; // current rotation matrix (double for Trackball)
	float _rotatef[16]; // current rotation matrix (float for OpenGL ES)
	double _quaternion[4]; // current quaternion used for rotation
	movePosition _start, _previous, _current; // store informations about user interactions
	int _width, _height; // size of OpenGL context in pixel
	float _left, _right, _top, _bottom; // value of "border"
	bool _gradiant, // show the background gradiant
	_showGeom, // show the Geometry
	_showMesh, // show the Mesh
	_fillMesh; // fill the Mesh
    
	void OrthofFromGModel(void);
	void drawPView(PView *p);
	void drawVectorArray(PViewOptions *opt, VertexArray *va);
    
public:
	drawContext();
	~drawContext(){}
	void load(std::string filename);
	void eventHandler(int event, float x=0, float y=0);
	void setQuaternion(double q0, double q1, double q2, double q3);
	void addQuaternion(double p1x, double p1y, double p2x, double p2y);
	void buildRotationMatrix();
	void setTranslate(int i, float t) {if(i>=0 && i<3) this->_translate[i] = t;}
	float getTranslate(int i) {if(i>=0 && i<3) return this->_translate[i]; return 0;}
	void setScale(int i, float s) {if(i>=0 && i<3) this->_scale[i] = s;}
	float getScale(int i) {if(i>=0 && i<3) return this->_scale[i]; return 0;}
	void initView(int w, int h);
	void drawView();
	void drawAxes(double x0=0., double y0=0., double z0=0., double h=0.5);
	void drawGeom();
	void drawMesh();
	void drawPost();
	void drawScale();
	void useGradiant(bool gradiant=true) {_gradiant = gradiant;}
	void showGeom(bool show=true) {_showGeom = show;}
	void showMesh(bool show=true) {_showMesh = show;}
	void fillMesh(bool show=true) {_fillMesh = show;}
    bool isShowedMesh(){return _showMesh;}
    bool isShowedGeom(){return _showGeom;}
};

#endif
