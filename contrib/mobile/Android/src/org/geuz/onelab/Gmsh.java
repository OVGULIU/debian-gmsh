package org.geuz.onelab;

import android.os.Handler;

public class Gmsh {
	/** From C / C++ code **/
	static {
		System.loadLibrary("f2cblas");
		System.loadLibrary("f2clapack");
		System.loadLibrary("petsc");
		System.loadLibrary("Gmsh");
		System.loadLibrary("GetDP");
		System.loadLibrary("Onelab");
        
    }
	private native long init(String name); // Init Gmsh
	private native void loadFile(long ptr, String name); // load a file(OpenProjet)
	private native void initView(long ptr, int w, int h); // Called each time the GLView change
	private native void drawView(long ptr); // Called each time the GLView request a render
	private native void setTranslation(long ptr, float tx, float ty, float tz); // translate the current GModel
	private native void setScale(long ptr, float sx, float sy, float sz); // scale the current GModel
	private native void setRotate(long ptr, float rx, float ry, float rz); // rotate the current GModel
	private native void setShow(long ptr, String what, boolean show); // select what to show / hide
	private native long getOnelabInstance(); // return the singleton of the onelab server
	public native String[] getParams(); // return the parameters for onelab
	public native int setParam(String type, String name, String value); // change a parameters
	public native String[] getPView(); // get a list of PViews
	public native void setPView(int position, int intervalsType,int visible,int nbIso); // Change options for a PView
	public native int onelabCB(String action); // Call onelab
	
	/** Java CLASS **/
	private long ptr;
	private long onelab;
	private Handler handler;

	public Gmsh(String name, Handler handler) {
		ptr = this.init(name);
		onelab = this.getOnelabInstance();
		this.handler = handler;
	}

	public void viewInit(int w, int h) {
		this.initView(ptr, w, h);
	}

	public void viewDraw() {
		this.drawView(ptr);
	}

	public void load(String filename){
		this.loadFile(ptr, filename);
	}

	public void translation(float tx, float ty, float tz)
	{
		this.setTranslation(ptr, tx, ty, tz);
	}

	public void scale(float sx, float sy, float sz)
	{
		this.setScale(ptr, sx, sy, sz);
	}

	public void rotate(float rx, float ry, float rz) {
		this.setRotate(ptr, rx, ry, rz);
	}
	public void showGeom(boolean show)
	{
		this.setShow(ptr, "geom", show);
	}
	public void showMesh(boolean show)
	{
		this.setShow(ptr, "mesh", show);
	}
	public long getOnelab() {
		return this.onelab;
	}
	public void ShowPopup(String message) {
		handler.obtainMessage(0, message).sendToTarget();
	}
	public void RequestRender() {
		handler.obtainMessage(1).sendToTarget();
	}
	public void SetLoading(String message) {
		handler.obtainMessage(2, message).sendToTarget();
	}
	public void SetLoading(int percent) {
		handler.obtainMessage(3, percent, 100).sendToTarget();
	}
}
