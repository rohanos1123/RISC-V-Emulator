#pragma once

#include <bitset>
#include <cstdint> 
#include <stdexcept>
#include <string>
#include <unordered_map> 
#include <iostream> 


#define R_TYPE_OPCODE           0b0110011
#define I_TYPE_MATH_OPCODE      0b0010011
#define I_TYPE_LOAD_OPCODE      0b0000011
#define I_TYPE_JALR_OPCODE      0b1100111
#define S_TYPE_OPCODE           0b0100011
#define SB_TYPE_OPCODE          0b1100011
#define U_TYPE_OPCODE           0b0110111
#define J_TYPE_OPCODE           0b1101111
#define EXIT_SIM_OPCODE         0b1111111
#define NOP_OPCODE              0b0000000


struct Instruction{
    uint8_t opcode = 0; 
    uint8_t rd = 0; 
    uint8_t funct3 = 0;
    uint8_t rs1 = 0;
    uint8_t rs2 = 0; 
    uint8_t funct7 = 0; 
    int32_t immediate = 0; 
    uint64_t address = 0; 
}; 

const std::unordered_map<std::string, Instruction> opcode_map = 
{{
    {"add",     Instruction{.opcode=R_TYPE_OPCODE, .funct3=0b000, .funct7=0b0000000}},
    {"sub",     Instruction{.opcode=R_TYPE_OPCODE, .funct3=0b000, .funct7=0b0100000}},
    {"addi",    Instruction{.opcode=I_TYPE_MATH_OPCODE, .funct3=0b000}},
    {"ld",      Instruction{.opcode=I_TYPE_LOAD_OPCODE, .funct3=0b010}},
    {"sd",      Instruction{.opcode=S_TYPE_OPCODE, .funct3=0b010}},
    {"lui",     Instruction{.opcode=U_TYPE_OPCODE}},
    {"and",     Instruction{.opcode=R_TYPE_OPCODE, .funct3=0b111, .funct7=0b0000000}},
    {"or",      Instruction{.opcode=R_TYPE_OPCODE, .funct3=0b110, .funct7=0b0000000}},
    {"xor",     Instruction{.opcode=R_TYPE_OPCODE, .funct3=0b100, .funct7=0b0100000}},
    {"andi",    Instruction{.opcode=I_TYPE_MATH_OPCODE, .funct3=0b111}},
    {"jal",    Instruction{.opcode=J_TYPE_OPCODE, .funct3=0b000}},
    {"jalr",   Instruction{.opcode=I_TYPE_JALR_OPCODE, .funct3=0b000}},
    {"beq",    Instruction{.opcode=SB_TYPE_OPCODE, .funct3=0b000}},   
    {"exit", Instruction{.opcode=EXIT_SIM_OPCODE, .funct3=0}},
    {"nop", Instruction{.opcode=NOP_OPCODE, .funct3=0}}

}}; 

inline void sign_extend(int32_t& value, int bits)
{
    uint32_t mask = -1 ^ ((1  << bits) - 1); 
    auto last_bit =  (value >> bits) & 0b1;
    value = (mask & (-last_bit)) | value;  
}


inline void decodeInstruction(const uint32_t& instr, Instruction &decoded){
    decoded.opcode = instr & 0x7F; 
    switch(decoded.opcode){
        case R_TYPE_OPCODE:{
            decoded.rd = (instr >> 7) & 0x1F;
            decoded.funct3 = (instr >> 12) & 0b111; 
            decoded.rs1 = (instr >> 15) & 0x1F; 
            decoded.rs2 = (instr >> 20) & 0x1F; 
            decoded.funct7 = (instr >> 25) & 0x7F; 
            break;
        }
        case I_TYPE_LOAD_OPCODE:
        case I_TYPE_MATH_OPCODE: 
        case I_TYPE_JALR_OPCODE: 
        {
            decoded.rd = (instr >> 7) & 0x1F;
            decoded.funct3 = (instr >> 12) & 0b111; 
            decoded.rs1 = (instr >> 15) & 0x1F; 
            decoded.immediate = (instr >> 20);
            break; 
        }
        case S_TYPE_OPCODE:{
            decoded.immediate = (instr >> 7) & 0x1F; 
            decoded.funct3 = (instr >> 12) & 0b111; 
            decoded.rs1 = (instr >> 15) & 0x1F; 
            decoded.rs2 = (instr >> 20) & 0x1F; 
            decoded.immediate = decoded.immediate | ((instr >>  25) << 5); 
            sign_extend(decoded.immediate, 11); 
            break; 
        }
        case SB_TYPE_OPCODE: {
            decoded.immediate = decoded.immediate | (((instr >> 7) & 0b1) << 11);
            decoded.immediate  = decoded.immediate | (((instr >> 8) & 0xF) << 1); 
            decoded.funct3 = (instr >> 12) & 0b111; 
            decoded.rs1 = (instr >> 15) & 0x1F; 
            decoded.rs2 = (instr >> 20) & 0x1F; 
            decoded.immediate = decoded.immediate | (((instr >> 25) & 0x3F) << 5); 
            decoded.immediate = decoded.immediate | ((instr >> 31) << 12); 
            sign_extend(decoded.immediate, 12); 
            break; 
        }
        case U_TYPE_OPCODE:{
            decoded.rd = (instr >> 7) & 0x1F; 
            decoded.immediate = decoded.immediate | ((instr >> 12) << 12); 
            break; 
        }
        case J_TYPE_OPCODE:{
            decoded.rd = (instr >> 7) & 0x1F; 
            decoded.immediate = decoded.immediate | (((instr >> 12) & 0xFF) << 12); 
            decoded.immediate = decoded.immediate | (((instr >> 20) & 0b1) << 11); 
            decoded.immediate = decoded.immediate | (((instr >> 21) & 0x3FF) << 1); 
            decoded.immediate = decoded.immediate | (((instr >> 31) & 0b1) << 20); 
            sign_extend(decoded.immediate, 20); 
            break; 
        }
        case EXIT_SIM_OPCODE: 
            break; 
        case NOP_OPCODE: 
            break; 
        default:
            throw std::runtime_error("Incorrect OP Code of value : " + std::to_string(decoded.opcode)); 

    }
}; 

inline void encodeInstruction(const Instruction &decoded, uint32_t& instr){
    instr = instr | (decoded.opcode & 0x7F); 
    switch (decoded.opcode) {
        case R_TYPE_OPCODE: 
            instr = instr | ((decoded.rd & 0x1F) << 7);  
            instr = instr | ((decoded.funct3 & 0b111) << 12);
            instr = instr | ((decoded.rs1 & 0x1F) << 15); 
            instr = instr | ((decoded.rs2 & 0x1F) << 20); 
            instr = instr | ((decoded.funct7 & 0x7F) << 25);
            break;
        case I_TYPE_LOAD_OPCODE:
        case I_TYPE_MATH_OPCODE:
        case I_TYPE_JALR_OPCODE: 
            instr = instr | ((decoded.rd & 0x1F) << 7); 
            instr = instr | ((decoded.funct3 & 0b111) << 12); 
            instr = instr | ((decoded.rs1 & 0x1F) << 15); 
            instr = instr |((decoded.immediate & 0x7FF) << 20);  
            break;
        case S_TYPE_OPCODE:
            instr = instr | ((decoded.immediate & 0x1F) << 7); 
            instr = instr | ((decoded.funct3 & 0b111) << 12); 
            instr = instr | ((decoded.rs1 & 0x1F) << 15); 
            instr = instr | ((decoded.rs2 & 0x1F) << 20); 
            instr = instr | (((decoded.immediate >> 5 ) & 0x7F) << 25); 
            break; 
        case SB_TYPE_OPCODE:
            instr = instr | (((decoded.immediate >> 11) & 0b1)  << 7); 
            instr = instr | (((decoded.immediate >> 1)  &  0xF) << 8);
            instr = instr | ((decoded.funct3 & 0b111) << 12); 
            instr = instr | ((decoded.rs1 & 0x1F) << 15); 
            instr = instr | ((decoded.rs2 & 0x1F) << 20); 
            instr = instr | ((((decoded.immediate) >> 5) & 0x3F) << 25);
            instr = instr | ((decoded.immediate >> 12) & 0b1) << 31; 
            break;
        case U_TYPE_OPCODE: 
            instr = instr | ((decoded.rd & 0x1F) << 7); 
            instr = instr | ((decoded.immediate >> 12) & 0xFFFFF) << 12; 
            break; 
        case J_TYPE_OPCODE: 
            instr = instr | ((decoded.rd & 0x1F) << 7); 
            instr = instr | (((decoded.immediate >> 12) & 0xFF) << 12);
            instr = instr | (((decoded.immediate >> 11) & 0b1) << 20); 
            instr = instr | (((decoded.immediate >> 1) & 0x3FF) << 21); 
            instr = instr | (((decoded.immediate >> 20) & 0b1) << 31);
            break; 
        case EXIT_SIM_OPCODE: 
            break; 
        case NOP_OPCODE:
            break; 
        default:
            throw std::invalid_argument("Invalid OPCODE parsed!"); 
    }
}; 

inline void print_insruction_struct(const Instruction& instr){
    std::cout<<"address: "<<instr.address<<std::endl; 
    std::cout<<"opcode: "<<(int)instr.opcode<<std::endl; 
    std::cout<<"funct3: "<<(int)instr.funct3<<std::endl; 
    std::cout<<"func7: "<<(int)instr.funct7<<std::endl; 
    std::cout<<"immediate: " <<(int)instr.immediate<<std::endl; 
    std::cout<<"immediate Bits: " <<std::bitset<32>((int)instr.immediate)<<std::endl; 
    std::cout<<"rd: "<<(int)instr.rd<<std::endl; 
    std::cout<<"rs1: "<<(int)instr.rs1<<std::endl; 
    std::cout<<"rs2: "<<(int)instr.rs2<<std::endl; 
};
