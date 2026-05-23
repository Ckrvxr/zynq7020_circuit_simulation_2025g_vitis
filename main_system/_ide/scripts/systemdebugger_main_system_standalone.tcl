# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: C:\Users\Ckrvxr\Desktop\circuit_simulation\vitis\main_system\_ide\scripts\systemdebugger_main_system_standalone.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source C:\Users\Ckrvxr\Desktop\circuit_simulation\vitis\main_system\_ide\scripts\systemdebugger_main_system_standalone.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-HS3 210299453697" && level==0 && jtag_device_ctx=="jsn-JTAG-HS3-210299453697-23727093-0"}
fpga -file C:/Users/Ckrvxr/Desktop/circuit_simulation/vitis/main/_ide/bitstream/All_desing_wrapper.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw C:/Users/Ckrvxr/Desktop/circuit_simulation/vitis/All_desing_wrapper/export/All_desing_wrapper/hw/All_desing_wrapper.xsa -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source C:/Users/Ckrvxr/Desktop/circuit_simulation/vitis/main/_ide/psinit/ps7_init.tcl
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#1"}
dow C:/Users/Ckrvxr/Desktop/circuit_simulation/vitis/main/Debug/main.elf
configparams force-mem-access 0
bpadd -addr &main
