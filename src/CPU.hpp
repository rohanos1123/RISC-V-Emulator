#include <cstdint>
#include <ctime>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string>
#include <thread>
#include "Memory.hpp"
#include "InstructionSet.hpp"

#define ALUOP_ADD   0b00
#define ALUOP_SUB   0b01
#define ALUOP_RT    0b10


template <typename T> 
struct Clocked{
    public: 
        T curr; 
        T future;
        void tick(){
            curr = future; 
        }
}; 




class RISC_V_CPU{

    private: 


        struct IF_ID{
            uint64_t instr_address = 0; 
            uint32_t instruction = 0; 

            uint64_t pc = 0; 
        }; 

        struct ID_EX{
            bool exit = false; 
            uint64_t instr_address = 0; 

            uint8_t ctl_wb = 0; 
            uint8_t ctl_m = 0; 
            uint8_t ctl_ex = 0;  
            uint8_t funct3 = 0; 
            uint8_t funct7 = 0; 
            uint8_t wr_reg = 0; 
            uint8_t reg1 = 0;
            uint8_t reg2 = 0; 
            uint64_t pc = 0;
            uint64_t rd_reg1 = 0; 
            uint64_t rd_reg2 = 0; 
            uint64_t immediate = 0;  
        }; 

        struct EX_MEM{
            bool exit = false; 
            uint64_t instr_address = 0; 

            uint8_t ctl_wb = 0; 
            uint8_t ctl_m = 0; 
            uint8_t zero = 0;
            uint8_t wr_reg = 0;  
            uint64_t rd_reg2 = 0; 
            uint64_t alu_res = 0; 
            uint64_t jmp_addr = 0; 
        }; 


        struct MEM_WB{
            bool exit = false; 
            uint64_t instr_address = 0; 

            uint8_t ctl_wb = 0; 
            uint64_t alu_res = 0;  
            uint64_t rd_data = 0; 
            uint8_t wr_reg = 0; 
        }; 

    private: 

        /*
            Memory Units
        */

        std::shared_ptr<Memory> main_memory; 
        std::unique_ptr<Cache<2048, 4>> l1_cache; 
        std::unique_ptr<Cache<512, 2>> instr_cache; 
        Register<32> ProgramRegisters;
        

        /*
            Pipeline Registers
        */

        Clocked<IF_ID> if_id_reg; 
        Clocked<ID_EX> id_ex_reg; 
        Clocked<EX_MEM> ex_mem_reg; 
        Clocked<MEM_WB> mem_wb_reg; 
        Clocked<uint64_t> PC; 
        uint32_t cycle = 0; 
        Instruction instr;
        uint32_t clock_period = 0;  
        bool in_op = true; 
        
        uint64_t if_instr = 0; 
        Clocked<uint64_t> wb_instr; 
      


    private: 
        /*
            Atomic Instructions
        */



        void InstructionAccess(const uint64_t &address, uint32_t &out_instr){
            uint64_t new_val = 0; 
            this->instr_cache->mem_read_reg(address, sizeof(out_instr), new_val); 
            out_instr = (uint32_t)(new_val); 
        }


        void ALU(const uint64_t& arg1, const uint64_t& arg2, const uint8_t ALU_ctl, uint8_t &out_zero, uint64_t &out_res){
            switch(ALU_ctl){
                case 0b0000:
                    out_res = arg1 & arg2; 
                    break; 
                case 0b0001: 
                    out_res = arg1 | arg2; 
                case 0b0010: 
                    out_res = arg1 + arg2; 
                    break; 
                case 0b0110: 
                    out_res = arg1 - arg2; 
                    break; 
                case 0b0111: 
                    out_res = arg1 < arg2; 
                    break; 
                case 0b1100:
                    out_res = ~(arg1 | arg2);
                    break; 
                case 0b1110:
                    out_res = arg1 >> arg2;
                    break;   
                case 0b1111: 
                    out_res = arg1 << arg2; 
                default: 
                    throw std::runtime_error("Invalid ALU ctl value " + std::to_string(ALU_ctl)); 
            }

            out_zero = (arg1 - arg2 == 0); 
        }


        void RegisterRead(const uint8_t& rd_reg1, const uint8_t& rd_reg2, uint64_t& rd_data1, uint64_t& rd_data2){
           
            rd_data1 = ProgramRegisters[rd_reg1]; 
            rd_data2 = ProgramRegisters[rd_reg2]; 

            std::cout<<"Read "<<rd_data1<<" from register "<<(int)rd_reg1<<std::endl; 
            std::cout<<"Read "<<rd_data2<<" from register "<<(int)rd_reg2<<std::endl; 
        }

        void RegisterWrite(const uint8_t& reg_write_ctl, const uint8_t& dest_reg, const uint64_t& write_data){            
            if(reg_write_ctl){
                std::cout<<"Writing "<<write_data<<" to reg "<<(int)dest_reg<<" in cycle "<<cycle<<std::endl; 
                ProgramRegisters[dest_reg] = write_data; 
            }
        }

        void MemoryAccess(const uint64_t& address, const uint64_t& data, const uint8_t& mem_read, const uint8_t& mem_write, const uint8_t& read_size, const uint8_t& write_sz, uint64_t& read_res){
            if(mem_read){
                l1_cache->mem_read_reg(address, read_size, read_res); 
            }

            if(mem_write){
                std::cout<<"Writing "<<data<<" to address "<<(int)address<<" in cycle "<<cycle<<std::endl; 
                l1_cache->mem_write_reg(address, write_sz, data); 
            }
        }
        
        void Control(const Instruction& instruction, uint8_t& exec_ctl, uint8_t& mem_ctl, uint8_t& wb_ctl){
            switch(instruction.opcode){
                case R_TYPE_OPCODE:
                    exec_ctl = 0b100;
                    mem_ctl = 0b000;
                    wb_ctl = 0b10;  
                    break;
                case I_TYPE_LOAD_OPCODE: 
                    exec_ctl = 0b001;
                    mem_ctl = 0b010;
                    wb_ctl = 0b11; 
                    break; 
                case I_TYPE_MATH_OPCODE:
                    exec_ctl = 0b101;
                    mem_ctl = 0b000;
                    wb_ctl = 0b10; 
                    break; 
                case S_TYPE_OPCODE:
                    exec_ctl = 0b001; 
                    mem_ctl = 0b001; 
                    wb_ctl = 0b00; 
                    break; 
                case SB_TYPE_OPCODE:
                    exec_ctl = 0b010;
                    mem_ctl  = 0b100; 
                    wb_ctl =  0b00; 
                    break; 
                case EXIT_SIM_OPCODE:
                    break; 
                case NOP_OPCODE:
                    break; 
                default:
                    throw std::invalid_argument("Unimplemented OPCODE of type: " + std::to_string(instruction.opcode)); 
            }
        }

        // Does writes to a temporary line (not a state element)
        uint8_t ALUControl(const uint8_t& ALU_op, const uint8_t& funct3, const uint8_t& funct7){
            switch(ALU_op){
                case ALUOP_ADD: 
                    return 0b0010; 
                    
                case ALUOP_SUB: 
                    return 0b110; 
                    
                case ALUOP_RT:
                {
                    switch(funct7){
                        case 0:
                            switch(funct3){
                                case 0b000:
                                    return 0b0010;
                                case 0b111: 
                                    return 0b0000;
                                case 0b110:
                                    return 0b0001;
                                default:
                                    throw std::invalid_argument("ALU control Invalid funct3"); 
                            }
                         
                        case 0b0100000: 
                            switch (funct3) {
                                case 000:
                                    return 0b0110; 
                                default:
                                    throw std::invalid_argument("ALU control Invalid funct3"); 
                            }
                        
                        default:
                            throw std::invalid_argument("ALU control Invalid funct7"); 
                    }
                }
                default:
                    throw std::invalid_argument("Invalid ALUOP Value From Control"); 
            }
        }

        void forwarding_unit(uint8_t &fwd_a, uint8_t &fwd_b){
            uint8_t ex_reg_write = (ex_mem_reg.curr.ctl_wb >> 1); 
            fwd_a = 0;
            fwd_b = 0; 
            if((ex_mem_reg.curr.wr_reg != 0) && ex_reg_write){
                auto ex_mem_rd = ex_mem_reg.curr.wr_reg; 
                if(id_ex_reg.curr.reg1 == ex_mem_rd){
                    fwd_a = 0b10;  
                    id_ex_reg.curr.rd_reg1 = ex_mem_reg.curr.alu_res; 
                }

                if(id_ex_reg.curr.reg2 == ex_mem_rd){
                    fwd_b = 0b10; 
                    id_ex_reg.curr.rd_reg2 = ex_mem_reg.curr.alu_res; 
                }
            }

            uint8_t mem_reg_write = (mem_wb_reg.curr.ctl_wb  >> 1); 
            if(mem_reg_write && (mem_wb_reg.curr.wr_reg != 0)){
                auto mem_wb_rd = mem_wb_reg.curr.wr_reg;
                if(id_ex_reg.curr.reg1 == mem_wb_rd){
                    fwd_a = 0b01; 
                    bool mem_to_reg = (mem_wb_reg.curr.ctl_wb & 0b1); 
                    if(mem_to_reg){
                        id_ex_reg.curr.rd_reg1 = mem_wb_reg.curr.rd_data;
                    }
                    else{
                        id_ex_reg.curr.rd_reg1 = mem_wb_reg.curr.alu_res; 
                    }
                }

                if(id_ex_reg.curr.reg2 == mem_wb_rd){
                    fwd_b = 0b01; 
                    bool mem_to_reg = (mem_wb_reg.curr.ctl_wb & 0b1); 
                    if(mem_to_reg){
                        id_ex_reg.curr.rd_reg2 = mem_wb_reg.curr.rd_data;
                    }
                    else{
                        id_ex_reg.curr.rd_reg2 = mem_wb_reg.curr.alu_res; 
                    }
                }
            }
        }

        void process_forward(uint64_t &read_val, const uint64_t& alternative, const uint8_t& fwd_code){
            switch(fwd_code){
                case 0:{
                    read_val = alternative; 
                    break;
                }
                case 0b01: {
                    std::cout<<"EX: MEM/WB Hazard Forwarded"<<std::endl; 
                    bool mem_to_reg = (mem_wb_reg.curr.ctl_wb & 0b1); 
                    if(mem_to_reg){
                        read_val = mem_wb_reg.curr.rd_data;
                    }
                    else{
                        read_val = mem_wb_reg.curr.alu_res; 
                    }
                    break; 
                }
                case 0b10:{ 
                    std::cout<<"EX: EX/MEM Hazard Forwarded"<<std::endl; 
                    read_val = ex_mem_reg.curr.alu_res; 
                    break; 
                }
                default:
                    throw std::invalid_argument("Unsupported data forwarding argument for A: " + std::to_string(fwd_code)); 
            }

        }


    private:
        /*
            Pipeline stages for the steps
        */
    

        void if_stage(){

            if_instr = PC.curr; 
            if_id_reg.future.instr_address = PC.curr; 

            if_id_reg.future.pc = PC.curr; 
            this->InstructionAccess(PC.curr, if_id_reg.future.instruction); 
        }

        void id_stage(){
            Instruction new_instr; 
            id_ex_reg.future.instr_address = if_id_reg.curr.instr_address; 
            
            decodeInstruction(if_id_reg.curr.instruction, new_instr); 
             
            if(new_instr.opcode == EXIT_SIM_OPCODE){
                id_ex_reg.future.exit = true; 
                return; 
            }
           
            RegisterRead(new_instr.rs1, new_instr.rs2,id_ex_reg.future.rd_reg1, id_ex_reg.future.rd_reg2);
            id_ex_reg.future.immediate = new_instr.immediate; 
            id_ex_reg.future.pc = PC.curr; 
            id_ex_reg.future.wr_reg = new_instr.rd; 
            id_ex_reg.future.reg1 = new_instr.rs1;
            id_ex_reg.future.reg2 = new_instr.rs2; 


            Control(new_instr, id_ex_reg.future.ctl_ex, id_ex_reg.future.ctl_m, id_ex_reg.future.ctl_wb); 
            id_ex_reg.future.funct3 = new_instr.funct3; 
            id_ex_reg.future.funct7 = new_instr.funct7; 
        }

        void ex_stage(){
            ex_mem_reg.future.instr_address = id_ex_reg.curr.instr_address; 

            uint8_t fwd_a = 0;
            uint8_t fwd_b = 0;  
            forwarding_unit(fwd_a, fwd_b); 

            ex_mem_reg.future.ctl_m = id_ex_reg.curr.ctl_m;
            ex_mem_reg.future.ctl_wb = id_ex_reg.curr.ctl_wb; 
            ex_mem_reg.future.rd_reg2 = id_ex_reg.curr.rd_reg2; 
            ex_mem_reg.future.exit = id_ex_reg.curr.exit; 

            ex_mem_reg.future.wr_reg = id_ex_reg.curr.wr_reg; 
            uint8_t alu_op = (id_ex_reg.curr.ctl_ex >> 1); 
            uint8_t alu_src = (id_ex_reg.curr.ctl_ex & 0b1); 

            uint8_t alu_cont = ALUControl(alu_op, id_ex_reg.curr.funct3, id_ex_reg.curr.funct7);
   
            uint64_t second_value = 0; 
                 
         
            if(alu_src){
                second_value = id_ex_reg.curr.immediate;
            }
            else{
                second_value = id_ex_reg.curr.rd_reg2; 
            }

            ALU(id_ex_reg.curr.rd_reg1, second_value, alu_cont, ex_mem_reg.future.zero, ex_mem_reg.future.alu_res); 
            ex_mem_reg.future.jmp_addr = PC.curr + (id_ex_reg.curr.immediate << 1); 
            
        }

        void mem_stage(){
            mem_wb_reg.future.instr_address = ex_mem_reg.curr.instr_address; 

            if(ex_mem_reg.curr.exit){
                in_op = false; 
                return; 
            } 
         
            uint8_t branch_signal = (ex_mem_reg.curr.ctl_m >> 2); 

            if(ex_mem_reg.curr.zero && branch_signal){
                PC.future = ex_mem_reg.curr.jmp_addr; 
            }
            else{
                PC.future = PC.curr + 4; 
            }

            mem_wb_reg.future.ctl_wb = ex_mem_reg.curr.ctl_wb; 
            mem_wb_reg.future.alu_res = ex_mem_reg.curr.alu_res; 
            mem_wb_reg.future.wr_reg = ex_mem_reg.curr.wr_reg; 
             
            uint8_t mem_read = (ex_mem_reg.curr.ctl_m >> 1) & 0b1; 
            uint8_t mem_write = (ex_mem_reg.curr.ctl_m) & 0b1; 

            // TODO: CHANGE depending on instruction
            const uint8_t write_sz = 8;
            const uint8_t read_sz = 8;
            MemoryAccess(ex_mem_reg.curr.alu_res, ex_mem_reg.curr.rd_reg2, mem_read, mem_write, read_sz, write_sz, mem_wb_reg.future.rd_data);
        }

        void wb_stage(){
            wb_instr.future = mem_wb_reg.curr.instr_address;

            auto reg_write_cd = (mem_wb_reg.curr.ctl_wb >> 1) & 0b1; 
            uint8_t mem_to_reg =  mem_wb_reg.curr.ctl_wb & 0b1; 
            uint64_t write_val = 0; 

            if(mem_to_reg){
                write_val = mem_wb_reg.curr.rd_data; 
            }
            else{
                write_val = mem_wb_reg.curr.alu_res; 
            }

            RegisterWrite(reg_write_cd, mem_wb_reg.curr.wr_reg, write_val); 
        }


    public: 
        RISC_V_CPU(std::shared_ptr<Memory> main, uint32_t clk_per)
        : l1_cache(std::make_unique<Cache<2048, 4>>(main, "l1_cache")), 
        instr_cache(std::make_unique<Cache<512, 2>>(main, "instr_cache")),
        clock_period(clk_per)
        {}        

        void set_pc(const uint64_t& set_pc){
            PC.curr = set_pc; 
            in_op = true; 
        }

        void advance_cycle(){
        
            wb_stage();
            if_stage();
            id_stage(); 
            ex_stage(); 
            mem_stage();    

            if_id_reg.tick();
            id_ex_reg.tick();
            ex_mem_reg.tick();
            mem_wb_reg.tick();
            PC.tick(); 
            wb_instr.tick(); 

             cycle++;  

            print_pipeline(); 
        }

        void print_pipeline(){
            std::cout<<"CYCLE: "<<cycle<<std::endl; 
            std::cout<<"IF: "<<if_id_reg.curr.instr_address<<std::endl;
            std::cout<<"ID: "<<id_ex_reg.curr.instr_address<<std::endl;  
            std::cout<<"EX: "<<ex_mem_reg.curr.instr_address<<std::endl; 
            std::cout<<"MEM: "<<mem_wb_reg.curr.instr_address<<std::endl; 
            std::cout<<"WB: "<<wb_instr.curr<<std::endl;  
            std::cout<<"---------------------------------------------"<<std::endl; 
        }

        void run(){
            while(in_op){
                advance_cycle(); 
                std::this_thread::sleep_for(std::chrono::milliseconds(clock_period)); 
            }
        }
}; 

