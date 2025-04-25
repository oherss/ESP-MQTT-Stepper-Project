# ESP MQTT Stepper Project

This project is an integration between the TMC 2209 stepper driver with an ESP 32-S3 chip and code that allows the motor to connect via MQTT with a custom protocol.
The code parts of this project are made to be compatible with the PD Stepper hardware project.
The hardware is also inspired by this project, but without some of the extra bells and whistles, like PD power, expansion ports and buttons.

# This is a student project
Which means I am doing this for an exam and may not maintain this repo in the future, this is mostly a proof-of-concept.
This also means the directories may be kind of confusing, and there is also some extra code in here for an unrelated project, so don't mind my mess :)

# Hardware
The hardware, just like the PD-Stepper project is meant to be attached to the back of an ordinary Nema 17 stepper motor.
# PCB
As mentioned, this is based on the PD-Stepper project, but with just the essentials, mostly to make it cheaper to produce.
This means we still get the TMC 2209 silent stepper motor driver, and the flexible ESP-32-s3 microprocessor, but I have done away with the I2C and AUX port, USB-C Power Delivery and I have also replaced the unobtainum power-regulator (MPM-3612-33) and replaced it with a commonplace alternative.
In the PCB folder I have both the fabrication files, complete with BOM and assemply files, so you don't need to solder the components yourself.
The files have been prepared for them to be produced via JLC-PCB, so if you want to use another fabricator, the files may not work.
There is also the project files for KiCAD 7.0 files to work on the schematic if you'd like, although I have a few components with custom libraries, which might make it annoying to initially set up.
# Other Hardware
In the future I'd like to get some 3D-models for 3D-printing,so one could incase the driver on the back of the stepper motor. And more importantly, I'd like a insulation plate between the PCB and back of the motor, to avoid short-circuits.
I'd also like to make plans for a cable to connect the motor to the PCB, like there is included with the PD-Stepper.

# Software
I have 2 versions of the code for the stepper motor driver, but they are in very different states of development, the basic version was the first draft, to test the capabilities.
The advanced uses the serial communication with the driver, but also has many updates on the protocol, so I would generally reccomend that version.