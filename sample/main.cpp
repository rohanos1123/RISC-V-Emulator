#include "../src/Assembler.hpp"
#include "../src/CPU.hpp"
#include <cstdint>
#include <memory>

int main(int argc, char** argv){
    if(argc < 2){
        std::cout<<"Usage: Include an assembly file to be compiled."<<std::endl; 
        return 0; 
    }

    Assembler asms(argv[1]);
    asms.read_source(); 
    auto phy_mem = std::make_shared<PhysicalMemory>("PhysicalMemory");
    RISC_V_CPU riscv(phy_mem, 200); 
    auto instr_vector = asms.get_instructions(); 
    phy_mem->write_buffer((char*)instr_vector.data(), 128, instr_vector.size() * sizeof(uint32_t)); 
    riscv.set_pc(128);    
    riscv.run(); 

    uint64_t value = 0;
    uint64_t k = 300; 
    phy_mem->mem_read_reg(k, 8, value); 
    std::cout<<"Should be 20: "<<value<<std::endl; 

    uint64_t value2 = 0;
    uint64_t k2 = 328; 
    phy_mem->mem_read_reg(k2, 8, value2); 
    std::cout<<"Should be 30: "<<value2<<std::endl; 
}
