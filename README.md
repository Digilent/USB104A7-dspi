USB104A7 DSPI Demo
====================

Description
-----------

This project demonstrates how to use Adept DSPI with the USB104A7 board. This demo comes in two parts, the FPGA project and the PC console application. Vivado is used to build the hardware platform and Xilinx Vitis is used to program the microblaze processor with the demo C/C++ application. The DSPI port provides a USB connected SPI interface between the PC software and FPGA logic. This interface connection sends and receives data to the USB104A7 running the demo application.

| Command			       | Function						                                                                  |
| ---------------------    | ------------------------------------------------------------------------------------------------ |
| write [hex bytes]   | write arbitrary bytes to device. IE: write 01 aa 3a 4b 77  |
| read [length]					   | reads [length] bytes from device. Read data is a counter  |


Requirements
------------
* **USB104A7**: To purchase a USB104A7, see the [Digilent Store](https://store.digilentinc.com/usb104a7/)
* **Vivado 2020.1 Installation with Vitis**: To set up Vivado, see the [Installing Vivado and Digilent Board Files Tutorial](https://reference.digilentinc.com/vivado/installing-vivado/start).
* **Adept Runtime**: Download links available on the [Adept Wiki](https://reference.digilentinc.com/reference/software/adept/start)
* **Serial Terminal Emulator Application (Optional)**: For more information see the [Installing and Using a Terminal Emulator Tutorial](https://reference.digilentinc.com/learn/programmable-logic/tutorials/tera-term).
* **USB A Cable**
* **5V DC Adapter (Or powered USB hub)**
* **Visual Studio Code Installed (Optional)** : Used to build the console application.
* **gcc compiler (mingw or linux) in path (Optional)** : Used to build the console application. On windows, gcc can be installed through mingw.

Demo Setup
----------

##### FPGA Project
1. Download the most recent release ZIP archives ("USB104A7-dspi-hw-*.zip", "USB104A7-dspi-sw-*.zip", "USB104A7-dspi-DemoApp.zip") from the repo's [releases page](https://github.com/Digilent/USB104A7-dspi/releases).
2. Extract the downloaded hw ZIP.
3. Open the XPR project file, \<archive extracted location\>/USB104A7-dspi/USB104A7_dspi.xpr, included in the extracted release archive in Vivado 2020.1.
4. Launch Vitis. When prompted for a workspace, select a new folder as a workspace.
5. Once the workspace opens, click the **Import** button. In the resulting dialog, first select *Vitis project exported zip file*, then click **Next**. Browse to and select the "USB104A7-dspi-sw-*.zip" folder.
6. Select all System Projects and Platform Projects and click Finish.
7. Plug in the 5v DC adapter and connect the USB104A7 to the PC using the USB cable.
8. Open a serial terminal application (such as [TeraTerm](https://ttssh2.osdn.jp/index.html.en) and connect it to the USB104A7's serial port, using a baud rate of 115200.
9. In the toolbar at the top of the Vitis window, select *Xilinx -> Program FPGA*. Leave all fields as their defaults and click "Program".
10. In the Project Explorer pane, right click on the "USB104A7_dspi_system" application project and select "Run As -> Launch on Hardware (System Debugger)".
11. The application will now be running on the USB104A7. It can be interacted with as described in the first section of this README using the Console Application described below.

##### Building the Console Application using VS Code
1. Extract "USB104A7-dspi-DemoApp.zip".
1. Open Visual Studio Code.
2. Open the extracted folder containing the Console Application in visual studio code.
3. To build, click Terminal -\> Run Build Task. This will run the build task found in tasks.json. This will run "gcc USB104A7_DSPI_DemoApp.c -g3 -O0 -o \<dir\>\\USB104A7_DSPI_DemoApp.exe -L./ -ldspi -ldmgr"
NOTE: The linux dpti and dmgr shared objects can be downloaded from the [Adept 2](https://reference.digilentinc.com/reference/software/adept/start) wiki page under Runtime - Latest Downloads.

Next Steps
----------
This demo can be used as a basis for other projects by modifying the hardware platform in the Vivado project's block design or by modifying the Vitis application project.

Check out the USB104A7's [Resource Center](https://reference.digilentinc.com/reference/programmable-logic/USB104A7/start) to find more documentation, demos, and tutorials.

For technical support or questions, please post on the [Digilent Forum](forum.digilentinc.com).

Additional Notes
----------------
For more information on how this project is version controlled, refer to the [digilent-vivado-scripts repo](https://github.com/digilent/digilent-vivado-scripts) and the [digilent-vitis-scripts repo](https://github.com/digilent/digilent-vitis-scripts).
