
module top(input clk, output LED1, output LED2, output LED3, output LED4, output LED5);

   reg ready = 0;
   reg [23:0] divider;
   reg [3:0] spin;

   always @(posedge clk) begin
      if (ready)
        begin
           if (divider == 6000000)
             begin
                divider <= 0;
                spin <= {spin[2], spin[3], spin[0], spin[1]};
             end
           else
             divider <= divider + 1;
        end
      else
        begin
           ready <= 1;
           spin <= 4'b1010;
           divider <= 0;
        end
   end

   assign LED1 = spin[0];
   assign LED2 = spin[1];
   assign LED3 = spin[2];
   assign LED4 = spin[3];
   assign LED5 = 1;
endmodule
