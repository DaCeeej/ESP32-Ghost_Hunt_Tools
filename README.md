# ESP32-Ghost_Hunt_Tools
Incomplete Arduino Code for "Phasmophobia" ghost hunting tools run on esp32 devices

This repository will hold all of the code written for some "ghost hunting tools" from the Phasmophobia video game that I've replicated in real life. I have 4 Firebeetle ESP32 boards that I've used. 1 is the ghost and the other 3 for ghost hunting tools (EMF reader, spirit box, IR thermometer, and eventually a fake camera). The ghost is a BLE server and the tools are BLE clients that all connect to the server. I'm utilizing RSSI values in order to figure out how close the tools are to the ghost. The RSSI values bounce around quite a bit so I use a running average and filter out RSSI values that are too low.

This will all be updated at a later time, but for now I'm uploading a few pieces of code. Everything is technically working, but with some occasional errors that I'll work on sorting out. The code is a bit of a mess and I will get around to cleaning it up a bit. 
