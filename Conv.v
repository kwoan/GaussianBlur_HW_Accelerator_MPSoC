module conv(
	//Avalon MM I/F
    input	wire	[3:0]	addr,
	output 	reg 	[31:0] 	rdata,
	input 	wire 	[31:0] 	wdata,
	input 	wire 			cs,
	input	wire  			read,
	input 	wire 			write,
  
	//Avlaon clock & reset I/F 
	input wire clk, 
	input wire rst
);

reg		[7:0] a, b, c, d, e, f, g, h, i; // img pixel values 
wire	[15:0] sum; // sum of pixel x mask

//Gaussian mask와 img픽셀을 convolution한 결과를 계산한 후 총 값 filter의 총합인 16으로 나눈다. 
assign sum = (a + b * 2 + c + d * 2 + e * 4 + f * 2 + g + h * 2 + i) >> 4;

// mask 
// 1 2 1 
// 2 4 2 
// 1 2 1

// assign image pixel values to each reg 
always @ (posedge clk) begin
	if (rst) begin
		a <= 8'b0;	e <= 8'b0;	i <= 8'b0;
		b <= 8'b0;	f <= 8'b0;
		c <= 8'b0;	g <= 8'b0;
		d <= 8'b0; 	h <= 8'b0;
	end
	else if (cs & write) begin
		if (addr == 4'd0) a <= wdata[7:0];
		else if (addr == 4'd1) b <= wdata[7:0]; 
		else if (addr == 4'd2) c <= wdata[7:0]; 
		else if (addr == 4'd3) d <= wdata[7:0]; 
		else if (addr == 4'd4) e <= wdata[7:0];
		else if (addr == 4'd5) f <= wdata[7:0]; 
		else if (addr == 4'd6) g <= wdata[7:0]; 
		else if (addr == 4'd7) h <= wdata[7:0]; 
		else if (addr == 4'd8) i <= wdata[7:0];
	end
end

always @ (posedge clk) begin 
	if(cs & read) begin
		case(addr) //addr에 따라 reg에 data를 read한다.
			4'd0: rdata <= {24'd0, a}; 
			4'd5: rdata <= {24'd0, f}; 
			4'd1: rdata <= {24'd0, b}; 
			4'd6: rdata <= {24'd0, g}; 
			4'd2: rdata <= {24'd0, c}; 
			4'd7: rdata <= {24'd0, h}; 
			4'd3: rdata <= {24'd0, d}; 
			4'd8: rdata <= {24'd0, i}; 
			4'd4: rdata <= {24'd0, e}; 
			4'd9: rdata <= {16'd0, sum}; 
			default: rdata <= 32'd0;
		endcase 
	end
end
 
endmodule