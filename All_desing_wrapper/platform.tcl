# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\Users\Ckrvxr\Desktop\circuit_simulation\vitis\All_desing_wrapper\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\Users\Ckrvxr\Desktop\circuit_simulation\vitis\All_desing_wrapper\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {All_desing_wrapper}\
-hw {C:\Users\Ckrvxr\Desktop\circuit_simulation\All_desing_wrapper.xsa}\
-out {C:/Users/Ckrvxr/Desktop/circuit_simulation/vitis}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {hello_world}
platform generate -domains 
platform active {All_desing_wrapper}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick
platform generate
domain active {zynq_fsbl}
bsp reload
domain active {standalone_ps7_cortexa9_0}
bsp reload
domain active {zynq_fsbl}
bsp reload
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
domain active {standalone_ps7_cortexa9_0}
bsp reload
platform active {All_desing_wrapper}
domain create -name {freertos10_xilinx_ps7_cortexa9_1} -display-name {freertos10_xilinx_ps7_cortexa9_1} -os {freertos10_xilinx} -proc {ps7_cortexa9_1} -runtime {cpp} -arch {32-bit} -support-app {freertos_hello_world}
platform generate -domains 
platform write
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
domain active {freertos10_xilinx_ps7_cortexa9_1}
platform generate -quick
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1 
bsp reload
platform generate -domains 
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1,zynq_fsbl 
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform clean
platform generate
bsp reload
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform generate
platform active {All_desing_wrapper}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
bsp reload
domain active {zynq_fsbl}
bsp reload
domain active {standalone_ps7_cortexa9_0}
bsp reload
platform generate -domains 
bsp reload
domain active {freertos10_xilinx_ps7_cortexa9_1}
bsp reload
platform generate -domains 
bsp reload
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform clean
platform clean
platform clean
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform active {All_desing_wrapper}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
bsp reload
domain active {standalone_ps7_cortexa9_0}
bsp reload
domain active {zynq_fsbl}
bsp reload
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
domain active {freertos10_xilinx_ps7_cortexa9_1}
bsp reload
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform clean
platform generate
bsp reload
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1,zynq_fsbl 
platform active {All_desing_wrapper}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(1).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(2).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(2).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(4).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(4).xsa}
platform generate -domains 
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1,zynq_fsbl 
platform active {All_desing_wrapper}
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(5).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(4).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/All_desing_wrapper(4).xsa}
platform generate -domains 
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper.xsa}
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper-2.xsa}
platform generate -domains 
platform clean
platform generate
platform clean
platform generate
platform config -updatehw {C:/Users/Ckrvxr/Desktop/circuit_simulation/All_desing_wrapper-2.xsa}
platform clean
platform generate
platform clean
platform generate
platform clean
platform generate
platform clean
platform generate
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1,zynq_fsbl 
platform clean
platform generate
platform generate -domains standalone_ps7_cortexa9_0,freertos10_xilinx_ps7_cortexa9_1,zynq_fsbl 
platform clean
platform generate
platform clean
platform generate
platform active {All_desing_wrapper}
bsp reload
bsp reload
domain active {standalone_ps7_cortexa9_0}
bsp reload
domain active {zynq_fsbl}
bsp reload
platform generate -domains 
platform config -remove-boot-bsp
platform write
platform config -fsbl-elf {C:\Users\Ckrvxr\Desktop\circuit_simulation\vitis\main\Release\main.elf}
platform write
domain active {freertos10_xilinx_ps7_cortexa9_1}
bsp reload
bsp reload
platform generate -domains zynq_fsbl 
platform config -create-boot-bsp
platform write
domain active {zynq_fsbl}
bsp reload
platform generate -domains zynq_fsbl 
