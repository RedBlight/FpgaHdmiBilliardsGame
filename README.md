# FpgaHdmiBilliardsGame

A billiards game running on FPGA and displayed on monitor via HDMI connection.  

Here is a video (click to open it in YouTube):

[![Fpga Hdmi Billiards Game](https://img.youtube.com/vi/JKzJVJ0ooZg/0.jpg)](https://www.youtube.com/watch?v=JKzJVJ0ooZg)

## Our hardware?
We used PYNQ-Z1 board.  
It has ZYNQ processor system in it, and we do use it.  
We used Vivado to program our FPGA board.  

## How to set it up?

1) Open the project with Vivado.  
2) Click “Generate Bitstream”.  
3) Wait for it to finish.  
4) Export hardware (include bitstream).  
5) Launch SDK.  
6) Comment out all the contents of auto-generated “objectbuffer.c”.  
7) Compile it.  
8) Program FPGA.  
9) Run it!  
10) ???  
11) Profit.  

P.S.: An already generated bitsream is included within the project. So alternatively, you can just open
the project, launch SDK, program FPGA, and then run it. 