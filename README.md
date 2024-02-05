# prusaKRL
converting G2/G3 containing gcode generated by prusaSlicer 2.7.1 and postprocessed gcode by ArcWelder to KRL (Kuka Robot Language). The specific setup is an KR___ with KRL 1.4. Its tool is an CEAD extruder, controlled through an analogue line (0-1) which translates to an servo speed of 0-150 rpm. This is quite stupid postprocessor, i dont feel like reiventing the wheel with in grasshopper or sputcam. All key 3D-printing slicing functions are already layed out for us in prusaSlicer, Cura etc. The easier way here is to adapt exported gcode to useful code for BAAM.

This code is for openFrameWorks 11.2. It's very setup specific; use with caution. Most likely you will catch a bunch of erros in KUKA HMI as soon variables are most likely not called the same at your end. Read the code and adapt.

Our current setup has an Analog output of an external PLC hooked up to the CEAD extruder, a 0.0 - 1.0 scale is translated to 0 - 150 rpm on a Bosch Servo. This scale is calculated within our KUKA 'firmware' or variable setup, in this software we scale a requested volume per minute ((extrusion_width * extrusion_height) * travel_minute).
You can not assume this script works one on one on your setup, you most likely know this, but still, read. Especially if your world coordinates differ from ours you will get an arm knocked in your face.

# Compiled using OF 11.2
These files are intended to replace source and header files for openFrameworks. One could replace the emptyExample source files with these. If your building on make build system make sure to inclde ofxGui to your addons.make :)

# Future work
- Reorganize g-code recignition to more general machining function (make it more universal)
- Analyze heat-dissipation per layer (for 3D-printing this is key; previous layer(s) shouldnt be to hot or cold)
- Break-out forms of extrusion control (We have a specific way of controllingt (Analog (1-0)), but can bet different)
- Improve GUI based on above features (make WYSIWYG!)

# Funding
This script is product of an ongoing research by; The Fieldwork Company, NIOZ, University of Utrecht and Bureau Waardenburg.
