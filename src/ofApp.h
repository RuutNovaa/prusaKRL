#pragma once

#include "ofMain.h"
#include "ofxGui.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		std::vector<std::string>gCodeBuffer;
		std::vector<std::string>gCodeFilteredBuffer;
		std::vector<std::string>krlCodeBuffer;
		ofPolyline guiPoly;
		std::vector<ofVec3f>midPointCollection;
		
		ofxPanel menu;
		ofParameterGroup mFileMan;
		ofParameter<bool> mFileOpen;
		ofParameter<bool> mFileSave;
		ofParameter<bool> mFileProcess;

		void mFileOpenListener(bool& sender);
		void mFileSaveListener(bool& sender);
		void mFileProcessListener(bool& sender);

		ofParameterGroup mExtrusionMan;
		ofParameter<float> mExtLayerHeight;
		ofParameter<float> mExtLayerWidth;
		ofParameter<float> mExtVolumeRev;
		ofParameter<float> mExtCalculatedFC;

		void mExtCalculateExtrusionData(float &sender);

		ofParameterGroup mPrintPosition;
		ofParameter<ofVec2f>mPrintOrigin;
		ofParameter<float>mPrintHeightOffset;
		ofParameter<float>mPrintSpeed;
		

		ofEasyCam guiCam;
		bool guiToggleCodeView;
		unsigned int guiCodeViewPosition;

		std::string softwareDescription;
		bool infoToggle;

		ofImage logoImg;

};
