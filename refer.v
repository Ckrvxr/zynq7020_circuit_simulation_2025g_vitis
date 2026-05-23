/******************************************************************************
* @file    bram_2_rtl_ctrl.v
* @brief   BRAM ?????? (?????)
*
* ?????
* 1. [????] ???? BRAM ???? Delay ??????? BRAM ??????
* 2. [????] FIR ?????? AXI tready ??????????????
* 3. [????] ?? ADDR_STRIDE ???????????(AXI)?????
* 4. [????] ??? reload_fir_tdata ????? 16 ?? Bug?
* 5. [????] ???????????????(Bitwise)????????
******************************************************************************/

module bram_2_rtl_ctrl (
    // ==================== BRAM PORTB ?? ====================
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB EN" *)
    (* X_INTERFACE_PARAMETER = "MASTER_TYPE MASTER" *)
    output bram_en_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB DOUT" *)
    input [31:0] bram_rddata_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB DIN" *)
    output [31:0] bram_wrdata_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB WE" *)
    output [3:0] bram_we_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB ADDR" *)
    output [31:0] bram_addr_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB CLK" *)
    output bram_clk_b,
    (* X_INTERFACE_INFO = "xilinx.com:interface:bram:1.0 BRAM_PORTB RST" *)
    output bram_rst_b,

    // ==================== ????? ====================
    input clk,
    input res_n,

    // ==================== ?????? ====================
    // sel = 1 ??? A ???sel = 0 ??? B ??
    // ?????? bit[3] ??
    output sel,

    // ==================== AXI-Stream ?? ====================
    output [11:0] adc_gain_tdata,
    output reg adc_gain_tvalid,
    input adc_gain_tready,

    output [11:0] dac_gain_tdata,
    output reg dac_gain_tvalid,
    input dac_gain_tready,

    output [31:0] cofing_dds_tdata,
    output reg cofing_dds_tvalid,
    input cofing_dds_tready,

    input [31:0] sweep_result_tdata,
    input sweep_result_tvalid,
    output reg sweep_result_tready,

    output [15:0] reload_fir_tdata,
    output reg reload_fir_tvalid,
    output reg reload_fir_tlast,
    input reload_fir_tready,

    output [7:0] cofing_fir_tdata,
    output reg cofing_fir_tvalid,
    input cofing_fir_tready,

    output [15:0] delay_config_tdata,
    output reg delay_config_tvalid,
    input delay_config_tready
);

    // ==================== ?????? ====================
    localparam [31:0] ADDR_DDS    = 32'd0 * 4; // 0
    localparam [31:0] ADDR_GAIN   = 32'd1 * 4; // 4
    localparam [31:0] ADDR_SWEEP  = 32'd2 * 4; // 8
    localparam [31:0] ADDR_STATUS = 32'd3 * 4; // 12 (0x0C)
    localparam [31:0] ADDR_FIR    = 32'd4 * 4; // 16 (0x10)

    // ==================== ?????? ====================
    localparam POLL_INTERVAL = 10'd1023;

    // ==================== ????? ====================
    // ??????????? DELAY ? CAPTURE ??
    localparam [4:0]
        IDLE               = 5'd0,
        CHECK_STATUS       = 5'd1,
        CHECK_STATUS_DELAY = 5'd2,
        CHECK_STATUS_WAIT  = 5'd3,
        PARSE_STATUS       = 5'd4,
        READ_DDS           = 5'd5,
        READ_DDS_DELAY     = 5'd6,
        READ_DDS_WAIT      = 5'd7,
        SEND_DDS           = 5'd8,
        CLEAR_DDS          = 5'd9,
        READ_GAIN          = 5'd10,
        READ_GAIN_DELAY    = 5'd11,
        READ_GAIN_WAIT     = 5'd12,
        SEND_ADC_GAIN      = 5'd13,
        SEND_DAC_GAIN      = 5'd14,
        CLEAR_GAIN         = 5'd15,
        SEND_DELAY         = 5'd16,
        READ_FIR_START     = 5'd17,
        READ_FIR_DELAY     = 5'd18,
        READ_FIR_CAPTURE   = 5'd19,
        SEND_FIR_LOW       = 5'd20,
        SEND_FIR_HIGH      = 5'd21,
        CONFIG_FIR         = 5'd22,
        CLEAR_FIR          = 5'd23,
        WAIT_SWEEP         = 5'd24,
        WRITE_SWEEP        = 5'd25,
        CLEAR_SWEEP        = 5'd26;

    // ==================== ????? ====================
    reg [4:0]  state;              
    reg [9:0]  poll_counter;       
    reg [31:0] addr_counter;       
    reg [11:0] fir_count;          
    
    reg [31:0] dds_reg;            
    reg [11:0] adc_gain_reg;       
    reg [11:0] dac_gain_reg;       
    reg        sel_reg;            
    reg [15:0] delay_config_reg;   
    reg [31:0] fir_data_reg;       
    reg [31:0] status_reg;         
    reg [31:0] sweep_data_reg;     
    
    reg        bram_en_reg;        
    reg [31:0] bram_addr_reg;      
    reg [31:0] bram_wrdata_reg;    
    reg [3:0]  bram_we_reg;        

    // ==================== ?????? ====================
    assign bram_en_b     = bram_en_reg;
    assign bram_addr_b   = bram_addr_reg;
    assign bram_wrdata_b = bram_wrdata_reg;
    assign bram_we_b     = bram_we_reg;
    assign bram_clk_b    = clk;
    assign bram_rst_b    = ~res_n;
    assign sel           = sel_reg;
    
    assign adc_gain_tdata   = adc_gain_reg;
    assign dac_gain_tdata   = dac_gain_reg;
    assign cofing_dds_tdata = dds_reg;
    
    // ????FIR ??????????16???????16?
    assign reload_fir_tdata = (state == SEND_FIR_HIGH) ? fir_data_reg[31:16] : fir_data_reg[15:0];
    
    assign cofing_fir_tdata = 8'd1;
    assign delay_config_tdata = delay_config_reg;

    // ==================== ???? ====================
    always @(posedge clk or negedge res_n) begin
        if (!res_n) begin
            state <= IDLE;
            poll_counter <= 10'd0;
            addr_counter <= 32'd0;
            fir_count <= 12'd0;
            dds_reg <= 32'd0;
            adc_gain_reg <= 12'd0;
            dac_gain_reg <= 12'd0;
            sel_reg <= 1'b0;
            delay_config_reg <= 16'd0;
            fir_data_reg <= 32'd0;
            status_reg <= 32'd0;
            sweep_data_reg <= 32'd0;
            
            bram_en_reg <= 1'b0;
            bram_addr_reg <= 32'd0;
            bram_wrdata_reg <= 32'd0;
            bram_we_reg <= 4'b0;
            
            adc_gain_tvalid <= 1'b0;
            dac_gain_tvalid <= 1'b0;
            cofing_dds_tvalid <= 1'b0;
            sweep_result_tready <= 1'b0;
            reload_fir_tvalid <= 1'b0;
            reload_fir_tlast <= 1'b0;
            cofing_fir_tvalid <= 1'b0;
            delay_config_tvalid <= 1'b0;
            
        end else begin
            case (state)
                
                // ==================== IDLE????? ====================
                IDLE: begin
                    adc_gain_tvalid <= 1'b0;
                    dac_gain_tvalid <= 1'b0;
                    cofing_dds_tvalid <= 1'b0;
                    reload_fir_tvalid <= 1'b0;
                    reload_fir_tlast <= 1'b0;
                    cofing_fir_tvalid <= 1'b0;
                    delay_config_tvalid <= 1'b0;
                    
                    bram_en_reg <= 1'b0;
                    bram_we_reg <= 4'b0000; // ?????

                    if (poll_counter < POLL_INTERVAL) begin
                        poll_counter <= poll_counter + 1'b1;
                    end else begin
                        poll_counter <= 10'd0;
                        state <= CHECK_STATUS;
                    end
                end
                
                // ==================== ??????? ====================
                CHECK_STATUS: begin
                    bram_addr_reg <= ADDR_STATUS;
                    bram_en_reg <= 1'b1;
                    state <= CHECK_STATUS_DELAY; 
                end
                
                // ????BRAM ??????
                CHECK_STATUS_DELAY: begin
                    bram_en_reg <= 1'b0;
                    state <= CHECK_STATUS_WAIT;
                end
                
                CHECK_STATUS_WAIT: begin
                    status_reg <= bram_rddata_b; // ???????????
                    state <= PARSE_STATUS;
                end
                
                // ==================== ??????? ====================
                PARSE_STATUS: begin
                    sel_reg <= status_reg[3];
                    delay_config_reg <= status_reg[22:7];
                    
                    if (status_reg[0])       state <= READ_DDS;
                    else if (status_reg[1])  state <= READ_GAIN;
                    else if (status_reg[2])  state <= READ_FIR_START;
                    else if (status_reg[5])  state <= SEND_DELAY;
                    else if (status_reg[4])  state <= WAIT_SWEEP;
                    else                     state <= IDLE;
                end
                
                // ==================== DDS ?? ====================
                READ_DDS: begin
                    bram_addr_reg <= ADDR_DDS;
                    bram_en_reg <= 1'b1;
                    state <= READ_DDS_DELAY;
                end
                
                READ_DDS_DELAY: begin
                    bram_en_reg <= 1'b0;
                    state <= READ_DDS_WAIT;
                end
                
                READ_DDS_WAIT: begin
                    dds_reg <= bram_rddata_b;
                    state <= SEND_DDS;
                end
                
                SEND_DDS: begin
                    cofing_dds_tvalid <= 1'b1;
                    if (cofing_dds_tready) begin
                        state <= CLEAR_DDS;
                    end
                end
                
                // ????????????????????
                CLEAR_DDS: begin
                    cofing_dds_tvalid <= 1'b0;
                    bram_addr_reg <= ADDR_STATUS;
                    bram_wrdata_reg <= status_reg & ~(32'h0000_0001); // ??? bit 0
                    bram_en_reg <= 1'b1;
                    bram_we_reg <= 4'b1111;
                    state <= IDLE;
                end
                
                // ==================== ???? ====================
                READ_GAIN: begin
                    bram_addr_reg <= ADDR_GAIN;
                    bram_en_reg <= 1'b1;
                    state <= READ_GAIN_DELAY;
                end
                
                READ_GAIN_DELAY: begin
                    bram_en_reg <= 1'b0;
                    state <= READ_GAIN_WAIT;
                end
                
                READ_GAIN_WAIT: begin
                    adc_gain_reg <= bram_rddata_b[11:0];
                    dac_gain_reg <= bram_rddata_b[27:16];
                    state <= SEND_ADC_GAIN;
                end
                
                SEND_ADC_GAIN: begin
                    adc_gain_tvalid <= 1'b1;
                    if (adc_gain_tready) begin
                        state <= SEND_DAC_GAIN;
                    end
                end
                
                SEND_DAC_GAIN: begin
                    adc_gain_tvalid <= 1'b0;
                    dac_gain_tvalid <= 1'b1;
                    if (dac_gain_tready) begin
                        state <= CLEAR_GAIN;
                    end
                end
                
                CLEAR_GAIN: begin
                    dac_gain_tvalid <= 1'b0;
                    bram_addr_reg <= ADDR_STATUS;
                    bram_wrdata_reg <= status_reg & ~(32'h0000_0002); // ??? bit 1
                    bram_en_reg <= 1'b1;
                    bram_we_reg <= 4'b1111;
                    state <= IDLE;
                end
                
                // ==================== Delay ?? ====================
                SEND_DELAY: begin
                    delay_config_tvalid <= 1'b1;
                    if (delay_config_tready) begin
                        delay_config_tvalid <= 1'b0;
                        bram_addr_reg <= ADDR_STATUS;
                        // ?? bit 5 (delay??)???????????? bit 6 (???delay done??)
                        bram_wrdata_reg <= (status_reg & ~(32'h0000_0020)) | 32'h0000_0040;
                        bram_en_reg <= 1'b1;
                        bram_we_reg <= 4'b1111;
                        state <= IDLE;
                    end
                end
                
                // ==================== FIR ??????????????? ====================
                READ_FIR_START: begin
                    bram_addr_reg <= ADDR_FIR;
                    bram_en_reg <= 1'b1;
                    addr_counter <= ADDR_FIR; // ?????????
                    fir_count <= 12'd0;
                    state <= READ_FIR_DELAY;
                end
                
                READ_FIR_DELAY: begin
                    bram_en_reg <= 1'b0;
                    state <= READ_FIR_CAPTURE;
                end
                
                READ_FIR_CAPTURE: begin
                    fir_data_reg <= bram_rddata_b; // ??????????
                    state <= SEND_FIR_LOW;
                end
                
                SEND_FIR_LOW: begin
                    reload_fir_tvalid <= 1'b1;
                    reload_fir_tlast <= 1'b0;
                    if (reload_fir_tready) begin
                        reload_fir_tvalid <= 1'b0;
                        state <= SEND_FIR_HIGH;
                    end
                end
                
                SEND_FIR_HIGH: begin
                    reload_fir_tvalid <= 1'b1;
                    reload_fir_tlast <= (fir_count == 12'd2079) ? 1'b1 : 1'b0;
                    
                    if (reload_fir_tready) begin
                        reload_fir_tvalid <= 1'b0;
                        fir_count <= fir_count + 1'b1;
                        
                        if (fir_count == 12'd2079) begin
                            state <= CONFIG_FIR;
                        end else begin
                            // ????????????????????
                            bram_addr_reg <= addr_counter;
                            bram_en_reg <= 1'b1;
                            addr_counter <= addr_counter;
                            state <= READ_FIR_DELAY; // ???????????
                        end
                    end
                end
                
                CONFIG_FIR: begin
                    reload_fir_tvalid <= 1'b0;
                    reload_fir_tlast <= 1'b0;
                    cofing_fir_tvalid <= 1'b1;
                    if (cofing_fir_tready) begin
                        state <= CLEAR_FIR;
                    end
                end
                
                CLEAR_FIR: begin
                    cofing_fir_tvalid <= 1'b0;
                    bram_addr_reg <= ADDR_STATUS;
                    bram_wrdata_reg <= status_reg & ~(32'h0000_0004); // ??? bit 2
                    bram_en_reg <= 1'b1;
                    bram_we_reg <= 4'b1111;
                    state <= IDLE;
                end
                
                // ==================== ?????? ====================
                WAIT_SWEEP: begin
                    sweep_result_tready <= 1'b1;
                    if (sweep_result_tvalid) begin
                        sweep_data_reg <= sweep_result_tdata;
                        state <= WRITE_SWEEP;
                    end
                end
                
                WRITE_SWEEP: begin
                    sweep_result_tready <= 1'b0;
                    bram_addr_reg <= ADDR_SWEEP;
                    bram_wrdata_reg <= sweep_data_reg; 
                    bram_en_reg <= 1'b1;
                    bram_we_reg <= 4'b1111;
                    state <= CLEAR_SWEEP;
                end
                
                CLEAR_SWEEP: begin
                    // pipelined ??????BRAM???????sweep????
                    // ???????????status??BRAM?????
                    bram_addr_reg <= ADDR_STATUS;
                    bram_wrdata_reg <= status_reg & ~(32'h0000_0010); // ??? bit 4
                    bram_en_reg <= 1'b1;
                    bram_we_reg <= 4'b1111;
                    state <= IDLE;
                end
                
                // ==================== ???? ====================
                default: begin
                    state <= IDLE;
                end
                
            endcase
        end
    end

endmodule