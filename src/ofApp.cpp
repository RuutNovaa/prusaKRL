#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

	menu.setup("Menu");
	menu.setWidthElements(400);
	
	//General file management.
	mFileMan.setName("File management");
	mFileMan.add(mFileOpen.set("Open ArcWelded File", false));
	mFileMan.add(mFileSave.set("Save ArcWelded KRL", false));
	mFileMan.add(mFileProcess.set("Process current", false));
	menu.add(mFileMan);
	
	//Gui for managing extrusion params
	mExtrusionMan.setName("Extrusion management");
	mExtrusionMan.add(mExtLayerHeight.set("Layer height [mm]",1.0f,0.5f,5.0f));
	mExtrusionMan.add(mExtLayerWidth.set("Layer width [mm]", 5.0f, 3.0f, 15.0f));
	mExtrusionMan.add(mExtVolumeRev.set("Volume per rotation [cm3/rev]", 1.26f, 0.5, 5.0f));
	mExtrusionMan.add(mExtCalculatedFC.set("Calculated flowC [mult]",0.0f,0.0f,100.0f));
	menu.add(mExtrusionMan);

	//Gui for general print position
	mPrintPosition.setName("Geometrical management");
	mPrintPosition.add(mPrintOrigin.set("Print origin [mm]",ofVec2f(0,0),ofVec2f(0,0),ofVec2f(3000,3000)));
	mPrintPosition.add(mPrintHeightOffset.set("Print Z offset [mm]",0,0,1000.0f));
	mPrintPosition.add(mPrintSpeed.set("Print speed [m/s]", 0.0f, 0.0f, 0.2f));
	menu.add(mPrintPosition);
	
	//Load menu data before assigning callbacks!
	menu.loadFromFile(ofxPanelDefaultFilename);

	//Assign callbacks now!
	mFileOpen.addListener(this, &ofApp::mFileOpenListener);
	mFileSave.addListener(this, &ofApp::mFileSaveListener);
	mFileProcess.addListener(this, &ofApp::mFileProcessListener);

	//Include speed in general extrusion params, KUKA assumes all rates at 100% travel speed!
	mExtLayerHeight.addListener(this, &ofApp::mExtCalculateExtrusionData);
	mExtLayerWidth.addListener(this, &ofApp::mExtCalculateExtrusionData);
	mExtVolumeRev.addListener(this, &ofApp::mExtCalculateExtrusionData);
	mExtCalculatedFC.addListener(this, &ofApp::mExtCalculateExtrusionData);
	mPrintSpeed.addListener(this, &ofApp::mExtCalculateExtrusionData);

	guiCodeViewPosition = 0;

	float emptyTrigger = 0.0f;
	mExtCalculateExtrusionData(emptyTrigger);

	softwareDescription = "This software is intended to convert .gcode (PrusaSlicer 2.7.1) files into .src (KRL 1.4) files. It accepts\n";
	softwareDescription += "either pure G1/G0 files or files exported with G0/G1/G2/G3 from prusaSlicer. The intention is to reduce\n";
	softwareDescription += "the file size, so G2 and G3 are recommended. In spiral vase mode prusaSlicer does not support G2/G3 arcs \n";
	softwareDescription += "due to the change in Z. For those cases (spiral mode is recommended for BAAM) ArcWelder can be used as an \n";
	softwareDescription += "additional post-processor. \n\n";

	softwareDescription += "ArcWelder comes with its own set of variables; -z flag is necessary to allow arcs in three dimensions and as \n";
	softwareDescription += "we are printing on a quite large format -r flag can be bumped up. -r Determines the maximum toolpath deviation\n";
	softwareDescription += "allowed during conversion. Small -r is more arcs, big -r is less arcs, this tunes program size and resolution.\n\n";

	softwareDescription += "Please remember that this is just a conversion-program, probably with quircky behavior. It doesnt catch all \n";
	softwareDescription += "gcodes beheaviours and the GUI-preview is a limited tool not a WYSIWYG. Use at your own risk ;)\n\n";

	softwareDescription += "Use (h) key to hide this message";

	infoToggle = true;

	logoImg.load("tfc-logo.png");

}

void ofApp::mExtCalculateExtrusionData(float& sender) {
	std::cout << "Extrusion calculation callback" << std::endl;

	float eHeight = mExtLayerHeight.get();
	float eWidth = mExtLayerWidth.get();
	float mSpeed = mPrintSpeed.get();

	std::cout << "Inputs: " << eHeight << ", " << eWidth << ", " << mSpeed << std::endl;

	//Calculate surface of extrusion over Z-X, which is a slot. Take rectangular volume, subtract the round corners
	//by subtracting round layer height from square layer height. 
	float eSurface = (eHeight * eWidth) - ((eHeight * eHeight) - (PI * ((eHeight / 2) * (eHeight / 2))));
	
	//Calculate distance traveled per minute
	float dMinute = mPrintSpeed.get() * 60.0f;
	std::cout << "Traveled distance per minute [m/min]: " << dMinute << std::endl;

	//Multiply the distance traveled at max speed by theoretical extruded cross section
	float eVolumeMinute = (dMinute * 1000.0f) * eSurface;
	std::cout << "Extrusion volume at max speed [mm3/min]: " << eVolumeMinute << std::endl;
	
	//Put previous calculated volume in its final perspective for volume; convert from mm3 to cm3
	eVolumeMinute = eVolumeMinute / 1000.0f;
	std::cout << "Extrusion volume at max speed [cm3/min]: " << eVolumeMinute << std::endl;

	//Devide the requested volume per minute by the volume provided per rotation resulting in RPM
	float calculatedFeed = eVolumeMinute / mExtVolumeRev.get();
	std::cout << "Calculated feed [RPM] : " << calculatedFeed << std::endl;
	
	//Set this value; min 0, max 150 rpm, this will be reference on final export.
	mExtCalculatedFC.set(ofMap(calculatedFeed, 0, 150, 0, 1, true) / mPrintSpeed.get());

	

}

void ofApp::mFileOpenListener(bool& sender) {

	

	std::cout << "Open callback!" << std::endl;
	
	ofFileDialogResult res = ofSystemLoadDialog();
	if (res.bSuccess && !res.filePath.empty()) {

		std::string fExt = res.filePath.substr(res.filePath.length() - 6);
		if (fExt.find(".gcode") != std::string::npos) {


			gCodeBuffer.clear();
			gCodeFilteredBuffer.clear();
			krlCodeBuffer.clear();
			guiPoly.clear();
			midPointCollection.clear();

			std::cout << "Correct, gcode extension" << std::endl;
			ofBuffer cBuf = ofBufferFromFile(res.filePath);

			unsigned int lNum = 0;
			for (auto line : cBuf.getLines()) {
				
				if (!line.empty()) {
					
					if (line.at(0) == 'G' || line.at(0) == 'M') {

						gCodeBuffer.push_back(line);

					}
					else {

						std::cout << "Discarded line " << lNum << ": " << line << std::endl;

					}

				}

				lNum++;
			
			}

			std::cout << "Actual movement lines: " << gCodeBuffer.size();

		}
		else {

			std::cout << "Wrong file extension" << std::endl;
		
		}

	}
	else {

		std::cout << "No file selected" << std::endl;
	
	}

	mFileOpen.set(false);

}
void ofApp::mFileSaveListener(bool& sender) {
	std::cout << "Save callback!" << std::endl;

	ofFileDialogResult fRes = ofSystemSaveDialog("File destination", "src only");

	if (fRes.bSuccess && !fRes.filePath.empty()) {

		std::string fullSavePath = "";
		
		//Check for entered extension, if none existing or different change the file extension
		if(fRes.filePath.find('.') != std::string::npos){
			
			size_t extPos = fRes.filePath.find('.');
			std::string extType = fRes.filePath.substr(extPos);

			if (extType == ".src") {
				fullSavePath = fRes.filePath;
			}
			else {
				fullSavePath = fRes.filePath.substr(0, extPos) + ".src";

			}

		}
		else {
			fullSavePath = fRes.filePath + ".src";
		}

		//Check for illegal characters in the filename and change or remove them!
		//Apart from normal filename illegalness (!,/,\) Kuka doesnt like hyphens!
		size_t changeFrom = fullSavePath.find_last_of('\\');
		std::replace(fullSavePath.begin()+changeFrom, fullSavePath.end(), '-', '_');
		
		std::cout << "Save path / file: " << fullSavePath << std::endl;

		//Warn users for the possible dangers of this software!
		std::string warnText =	"WARNING! \n";
		warnText +=				"The gcode file will be converted to .src for use with the KUKA. \n";
		warnText +=				"Make sure of the following:\n";
		warnText +=				" - The origin of the print is set correctly, also assuming the exported gcode has origin 0,0,0 in the slicer\n";
		warnText +=				" - The calculated midpoints (red) of arcs make sense. Midpoints outside of paths indicate wrong calculation / arc direction\n";
		warnText +=				" - The Z-height is correct\n";
		warnText +=				" - To dry run your print on a low speed when being run for the first time!\n";
		warnText +=				" - Double check the code in preview, product of this program is your liability :')\n\n";
		warnText +=				"HAVE A NICE PRINT";

		guiToggleCodeView = true;

		ofSystemAlertDialog(warnText);

		//Compile KRL file!
		std::vector<std::string> krlOutput;

		krlOutput.push_back("DEF ofgen()");

		krlOutput.push_back("GLOBAL INTERRUPT DECL 3 WHEN $STOPMESS==TRUE DO IR_STOPM ( )");
		krlOutput.push_back("INTERRUPT ON 3");
		krlOutput.push_back("BAS (#INITMOV,0 )");

		krlOutput.push_back("ANOUT ON AO_EXTRUDER_RPM = FLOW_CORRECTION * $VEL_ACT +0.0 DELAY=-0.2");
		krlOutput.push_back("FLOW_CORRECTION = " + ofToString(mExtCalculatedFC.get(),3));

		krlOutput.push_back("$BWDSTART = FALSE");
		krlOutput.push_back("PDAT_ACT = {VEL 15,ACC 100,APO_DIST 50}");
		krlOutput.push_back("BAS(#PTP_DAT)");
		krlOutput.push_back("FDAT_ACT = {TOOL_NO 6,BASE_NO 0,IPO_FRAME #BASE}");
		krlOutput.push_back("BAS(#FRAMES)");

		krlOutput.push_back("BAS (#VEL_PTP,15)");
		krlOutput.push_back("PTP  {A1 5,A2 -90,A3 100,A4 5,A5 -10,A6 -5,E1 0,E2 0,E3 0,E4 0}");

		krlOutput.push_back("$VEL.CP=" + ofToString(mPrintSpeed.get(), 2));
		krlOutput.push_back("$ADVANCE=3");

		for (auto i : krlCodeBuffer) {

			krlOutput.push_back(i);

		}

		krlOutput.push_back("END");

		//Write file
		ofFile nFile = ofFile(fullSavePath, ofFile::Mode::Append, false);
		
		if (nFile.exists()) {
			nFile.remove();
		}
		
		if (nFile.create()) {
			if (nFile.open(fullSavePath, ofFile::Mode::Append, false)) {

				for (auto i : krlOutput) {
					nFile << i << std::endl;
				}

				nFile.close();

			}

		}

	}



	mFileSave.set(false);
}
void ofApp::mFileProcessListener(bool& sender) {
	std::cout << "Process callback!" << std::endl;

	std::string popupMsgString;
	popupMsgString = "Please understand this is a blocking function\n";
	popupMsgString += "Complex G1/G0 scripts take way longer, check\n";
	popupMsgString += "the console for actual action ;)";

	ofSystemAlertDialog(popupMsgString);

	if (!gCodeBuffer.empty()) {

		gCodeFilteredBuffer.clear();
		krlCodeBuffer.clear();
		guiPoly.clear();
		midPointCollection.clear();

		std::cout << "Start gCode to polyline conversion!" << std::endl;

		unsigned int lineNumber = 1;
		bool firstLinear = false;
		bool isExtruding = false;
		ofVec3f lastPosition = ofVec3f(0, 0, 0);
		ofVec3f currentPosition = ofVec3f(0, 0, 0);

		ofPolyline cPoly;

		for (auto line : gCodeBuffer) {


			bool processFlag = false;
			ofVec2f currentArcOffset = ofVec2f(0, 0);
			bool hasE = line.find('E') != std::string::npos;
			
			bool hasX = line.find('X') != std::string::npos;
			bool hasY = line.find('Y') != std::string::npos;
			bool hasZ = line.find('Z') != std::string::npos;
			
			bool hasI = line.find('I') != std::string::npos;
			bool hasJ = line.find('J') != std::string::npos;

			if (hasX) {
				currentPosition.x = std::stof(line.substr(line.find('X') + 1, (line.find(' ', line.find('X')) - (line.find('X') + 1))));
			}
			if (hasY) {
				currentPosition.y = std::stof(line.substr(line.find('Y') + 1, (line.find(' ', line.find('Y')) - (line.find('Y') + 1))));
			}
			if (hasZ) {
				currentPosition.z = std::stof(line.substr(line.find('Z') + 1, (line.find(' ', line.find('Z')) - (line.find('Z') + 1))));
			}

			if (hasI) {
				currentArcOffset.x = std::stof(line.substr(line.find('I') + 1, (line.find(' ', line.find('I')) - (line.find('I') + 1))));
			}
			if (hasJ) {
				currentArcOffset.y = std::stof(line.substr(line.find('J') + 1, (line.find(' ', line.find('J')) - (line.find('J') + 1))));
			}

			

			if (line.find("G0") != std::string::npos) {

				if (hasZ && !hasX && !hasY) {
					//Do nothing, just a position change if coordinate is new
					if (lastPosition.z != currentPosition.z) {
						
						std::cout << "Make new point, old z: " << lastPosition.z << ",new z: " << currentPosition.z << std::endl;
						cPoly.addVertex(currentPosition);

						//Switch extrusion on and off
						if (hasE && !isExtruding) {

							krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
							isExtruding = true;

						}

						if (!hasE && isExtruding) {

							krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
							isExtruding = false;
						}
						
						//Check if this is the first line, if so use PTP
						if (!firstLinear) {
							krlCodeBuffer.push_back("PTP {X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0, E1 0, E2 0, E3 0, E4 0, S 'B 110'} C_PTP");
							firstLinear = true;
						}
						else {

							krlCodeBuffer.push_back("LIN{ X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0 } C_DIS");

						}

						processFlag = true;

					}
					
				}
				if (hasX && hasY) {
					cPoly.addVertex(currentPosition);

					//Switch extrusion on and off
					if (hasE && !isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
						isExtruding = true;

					}

					if (!hasE && isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
						isExtruding = false;
					}

					//Check if this is the first line, if so use PTP
					if (!firstLinear) {
						krlCodeBuffer.push_back("PTP {X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0, E1 0, E2 0, E3 0, E4 0, S 'B 110'} C_PTP");
						firstLinear = true;
					}
					else {

						krlCodeBuffer.push_back("LIN{ X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0 } C_DIS");

					}

					processFlag = true;
				}

				
				
			}
			else if (line.find("G1") != std::string::npos) {

				if (hasZ && !hasX && !hasY) {
					//Do nothing, just a position change if coordinate is new
					if (lastPosition.z != currentPosition.z) {
						std::cout << "Make new point, old z: " << lastPosition.z << ",new z: " << currentPosition.z << std::endl;
						cPoly.addVertex(currentPosition);

						//Switch extrusion on and off
						if (hasE && !isExtruding) {

							krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
							isExtruding = true;

						}

						if (!hasE && isExtruding) {

							krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
							isExtruding = false;
						}

						//Check if this is the first line, if so use PTP
						if (!firstLinear) {
							krlCodeBuffer.push_back("PTP {X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0, E1 0, E2 0, E3 0, E4 0, S 'B 110'} C_PTP");
							firstLinear = true;
						}
						else {

							krlCodeBuffer.push_back("LIN{ X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0 } C_DIS");

						}
						
						processFlag = true;
					}
					
				}
				if (hasX && hasY) {
					cPoly.addVertex(currentPosition);

					//Switch extrusion on and off
					if (hasE && !isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
						isExtruding = true;

					}

					if (!hasE && isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
						isExtruding = false;
					}

					//Check if this is the first line, if so use PTP
					if (!firstLinear) {
						krlCodeBuffer.push_back("PTP {X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0, E1 0, E2 0, E3 0, E4 0, S 'B 110'} C_PTP");
						firstLinear = true;
					}
					else {

						krlCodeBuffer.push_back("LIN{ X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0 } C_DIS");

					}

					processFlag = true;
				}

				

			}
			else if (line.find("G21") != std::string::npos) {
				//Catch this exception, do nothing!
				std::cout << "G21 found at " << lineNumber << std::endl;
			}
			else if (line.find("G2") != std::string::npos) {
				
				//Arc clockwise
				if (hasI && hasJ && hasX && hasY) {


					
					//Gui calculations
					ofVec2f arcCent = ofVec2f(lastPosition.x + currentArcOffset.x, lastPosition.y + currentArcOffset.y);
					float arcRad = sqrt(pow(abs(currentArcOffset.x), 2) + pow(abs(currentArcOffset.y), 2));
					float startAngle = atan2(lastPosition.y - arcCent.y, lastPosition.x - arcCent.x) * (180 / PI);
					float endAngle = atan2(currentPosition.y - arcCent.y, currentPosition.x - arcCent.x) * (180 / PI);

					cPoly.arcNegative(arcCent.x, arcCent.y, currentPosition.z, arcRad, arcRad, startAngle, endAngle);
					

					//Switch extrusion on and off
					if (hasE && !isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
						isExtruding = true;

					}

					if (!hasE && isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
						isExtruding = false;
					}
					
					//Auxilary point calculations for KRL (midpoint)
					float startAngleRad = atan2(lastPosition.y - arcCent.y, lastPosition.x - arcCent.x);
					float endAngleRad = atan2(currentPosition.y - arcCent.y, currentPosition.x - arcCent.x);


					float a = startAngleRad - endAngleRad;
					float aDelta = atan2(sin(a), cos(a));

					
					if (aDelta < 0) {
						aDelta += 2 * PI;
					}

					float midAngleRad = startAngleRad - (aDelta / 2);
					float midPx = (cos(midAngleRad) * arcRad) + arcCent.x;
					float midPy = (sin(midAngleRad) * arcRad) + arcCent.y;
					float midPz = currentPosition.z - ((currentPosition.z - lastPosition.z) / 2);

					ofVec3f midPointCoord = ofVec3f(midPx, midPy, midPz);

					midPointCollection.push_back(midPointCoord);
					krlCodeBuffer.push_back("CIRC { X " + ofToString(midPointCoord.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(midPointCoord.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(midPointCoord.z + mPrintHeightOffset.get(),1) + "},{ X " + ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0} C_DIS");

				}
				else {

					std::cout << "Error; parameters for arc not found, ln: " << lineNumber << std::endl;
				}

				processFlag = true;

			}
			else if (line.find("G3") != std::string::npos) {

				//Arc counterclockwise
				if (hasI && hasJ && hasX && hasY) {

					//Gui calculations
					ofVec2f arcCent		= ofVec2f(lastPosition.x + currentArcOffset.x, lastPosition.y + currentArcOffset.y);
					float arcRad		= sqrt(pow(abs(currentArcOffset.x),2) + pow(abs(currentArcOffset.y),2));
					float startAngle	= atan2(lastPosition.y-arcCent.y,lastPosition.x-arcCent.x)*(180/PI);
					float endAngle		= atan2(currentPosition.y-arcCent.y, currentPosition.x-arcCent.x)*(180/PI);

					cPoly.arc(arcCent.x,arcCent.y,currentPosition.z,arcRad,arcRad,startAngle,endAngle);

					//Switch extrusion on and off
					if (hasE && !isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = TRUE");
						isExtruding = true;

					}

					if (!hasE && isExtruding) {

						krlCodeBuffer.push_back("TRIGGER WHEN DISTANCE = 0 DELAY = 0 DO O_EXTRUDER_START = FALSE");
						isExtruding = false;
					}

					//Auxilary point calculations for KRL (midpoint)
					float startAngleRad = atan2(lastPosition.y - arcCent.y, lastPosition.x - arcCent.x);
					float endAngleRad = atan2(currentPosition.y - arcCent.y, currentPosition.x - arcCent.x);
					
					
					float a = endAngleRad - startAngleRad;
					float aDelta = atan2(sin(a), cos(a));

					
					if (aDelta < 0) {
						aDelta += 2*PI;
					}

					float midAngleRad = startAngleRad + (aDelta / 2);
					float midPx = (cos(midAngleRad) * arcRad) + arcCent.x;
					float midPy = (sin(midAngleRad) * arcRad) + arcCent.y;
					float midPz = currentPosition.z - ((currentPosition.z - lastPosition.z) / 2);
					
					ofVec3f midPointCoord = ofVec3f(midPx,midPy,midPz);

					midPointCollection.push_back(midPointCoord);
					krlCodeBuffer.push_back("CIRC { X "+ofToString(midPointCoord.x + mPrintOrigin.get().x,1) + ", Y "+ofToString(midPointCoord.y + mPrintOrigin.get().y,1)+", Z "+ofToString(midPointCoord.z + mPrintHeightOffset.get(),1) + "},{ X "+ofToString(currentPosition.x + mPrintOrigin.get().x,1) + ", Y " + ofToString(currentPosition.y + mPrintOrigin.get().y,1) + ", Z " + ofToString(currentPosition.z + mPrintHeightOffset.get(),1) + ", A 0, B 90, C 0} C_DIS");

					

				}
				else {

					std::cout << "Error; parameters for arc not found, ln: " << lineNumber << std::endl;
				}

				processFlag = true;

			}
			else {

				//Do nothing for now

			}
			std::cout  << lineNumber << " ("<< processFlag <<": " << currentPosition.x << ", " << currentPosition.y << ", " << currentPosition.z << " :: " << line << std::endl;

			//Save for comparison with KRL in gui 
			if (processFlag) {
				gCodeFilteredBuffer.push_back(line);
			}

			//Save position for references in arcs
			lastPosition = currentPosition;
			lineNumber++;

		


		}

		guiPoly = cPoly;

	}
	else {

		std::cout << "gCode buffer is empty, stop." << std::endl;

	}

	mFileProcess.set(false);

}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){


	guiCam.begin();
	guiPoly.draw();

	ofSetColor(255, 0, 0);

	for (auto i : midPointCollection) {

		ofDrawSphere(i, 2);

	}

	ofSetColor(255, 255, 255);

	guiCam.end();

	if (guiToggleCodeView && gCodeFilteredBuffer.size() > 0) {

		ofRectangle gCodeView(ofGetWindowWidth() - 500, 15, 500, ofGetWindowHeight());
		ofRectangle krlCodeView(ofGetWindowWidth() - 1500, 15, 1000, ofGetWindowHeight());

		int viewPos = guiCodeViewPosition;

		if (guiCodeViewPosition < 0) {
			viewPos = 0;
		}
		if (guiCodeViewPosition > gCodeFilteredBuffer.size()) {
			viewPos = gCodeFilteredBuffer.size() - 1;
		}

		std::string gCodeViewString = "";
		for (int i = viewPos; i < gCodeFilteredBuffer.size(); i++) {
			gCodeViewString += ofToString(i) + ". " + gCodeFilteredBuffer.at(i) + '\n';
		}
		
		ofSetColor(0, 0, 0);
		ofDrawRectangle(gCodeView);
		ofSetColor(255, 255, 255);
		ofDrawBitmapString(gCodeViewString, gCodeView.getTopLeft().x, gCodeView.getTopLeft().y+30);

		std::string krlViewString = "";
		for (int i = viewPos; i < krlCodeBuffer.size(); i++) {
			krlViewString += ofToString(i) + ". " + krlCodeBuffer.at(i) + '\n';
		}

		ofSetColor(0, 0, 0);
		ofDrawRectangle(krlCodeView);
		ofSetColor(255, 255, 255);
		ofDrawBitmapString(krlViewString,krlCodeView.getTopLeft().x,krlCodeView.getTopLeft().y+30);
		
		std::string cViewInstruct = "Keys: (v) - toggle this code view, (+) - advance viewing line, (-) - subtract viewing line";
		ofDrawBitmapString(cViewInstruct, krlCodeView.getTopLeft().x, krlCodeView.getTopLeft().y + 15);

	}


	menu.draw();

	
	if(infoToggle) ofDrawBitmapStringHighlight(softwareDescription, 10, menu.getHeight() + 50);

	ofEnableAlphaBlending();
	logoImg.draw(ofGetWindowWidth()- (logoImg.getWidth() / 2)-30,ofGetWindowHeight()- (logoImg.getHeight() / 2)-10,logoImg.getWidth()/2,logoImg.getHeight()/2);
	ofDisableAlphaBlending();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if (key == '-') guiCodeViewPosition--;
	if (key == '+') guiCodeViewPosition++;
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
	if (key == 'v') guiToggleCodeView = !guiToggleCodeView;
	if (key == 'h') infoToggle = !infoToggle;
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
