#pragma once

#include <cctype>
#include <cstddef>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint> 
#include <regex> 
#include "InstructionSet.hpp"

class Assembler{
    private:
        struct LexedInstruction{
            std::vector<std::string> arg_list; 
            size_t instr_pos = 0; 
        }; 

        std::string filename; 
        std::ifstream source_file; 
        std::unordered_map<std::string, int> label_map; 
        std::vector<LexedInstruction> lexed_list; 
        std::vector<uint32_t> instructions; 
        std::vector<Instruction> instr_structs; 
     
       
        public:     
        Assembler(std::string fname)
        : filename(fname), source_file(fname)
        {
            if(!source_file.is_open()){
                throw std::invalid_argument("Failed to find file: " + fname); 
            }
        } 

        /*
            Extracts the index from the register
        */
        uint8_t register_extract(const std::string& reg_str){
            std::regex reg_rule("x(\\d+)");
            std::smatch match; 
            if(std::regex_search(reg_str, match, reg_rule)){
                uint8_t val = 0; 
                try{
                    val = std::stoi(match[1], nullptr, 0); 
                    if(val < 32 && val >= 0){
                        return val; 
                    }
                    else{
                        throw std::invalid_argument("Register value " + std::to_string(val) + " is not valid."); 
                    }
                }
                catch(std::exception e){
                    throw std::invalid_argument("Cannot convert string: " + reg_str + " to register_index"); 
                }   
            }
            else{
                 throw std::invalid_argument("Cannot convert string: " + reg_str + "to register_index");
            }
        }

        /*
            Extract data from memory extract immediate, register
        */
        std::pair<int32_t, uint8_t> mem_access_extract(const std::string& mem_access){
            std::regex reg_rule("(-?\\d+)\\(x(\\d+)\\)"); 
            std::smatch match; 
            if(std::regex_search(mem_access, match, reg_rule)){
                int32_t immediate = 0; 
                uint8_t register_num = 0; 
                try{
                    immediate = std::stoi(match[1], nullptr, 0); 
                    register_num = std::stoi(match[2]); 
                    return {immediate, register_num}; 
                }
                catch(std::exception e){
                    throw std::invalid_argument("Failure to extract memory information " + mem_access); 
                }   
            }
            else{
                throw std::invalid_argument("Failure to extract memory information " + mem_access); 
            }
        }

        /*
            Extracts the number from a number string (with )
        */
        int32_t immediate_extract(const std::string &immediate){
            int32_t immediate_val = 0; 
            try{
                immediate_val = std::stoi(immediate, nullptr, 0); 
                return immediate_val; 
            }
            catch(std::exception e){
                throw std::invalid_argument("Failure to extract immediate value " + immediate); 
            }
        }


        void parse_r_type(const LexedInstruction& lex, Instruction &instr){
            instr.rs1 = register_extract(lex.arg_list[2]);
            instr.rs2 = register_extract(lex.arg_list[3]);
            instr.rd  = register_extract(lex.arg_list[1]) ;
        }

        void parse_i_math_type(const LexedInstruction& lex, Instruction &instr){
            instr.rd = register_extract(lex.arg_list[1]);
            instr.rs1 = register_extract(lex.arg_list[2]);
            instr.immediate = immediate_extract(lex.arg_list[3]); 
        }

        void parse_i_load_type(const LexedInstruction& lex, Instruction &instr){
            instr.rd = register_extract(lex.arg_list[1]);
            auto [imm, rs1] = mem_access_extract(lex.arg_list[2]); 
            instr.rs1 = rs1; 
            instr.immediate = imm; 
        }

        void parse_i_jalr_type(const LexedInstruction& lex, Instruction &instr){
            instr.rd = register_extract(lex.arg_list[1]); 
            auto [imm, rs1] = mem_access_extract(lex.arg_list[2]);
            instr.rs1 = rs1; 
            instr.immediate = imm; 
        }

        void parse_s_type(const LexedInstruction& lex, Instruction& instr){
            auto [imm, base_reg] = mem_access_extract(lex.arg_list[2]);
            instr.rs1 = base_reg;
            instr.rs2 = register_extract(lex.arg_list[1]); 
            instr.immediate = imm; 
        }

        void parse_sb_type(const LexedInstruction& lex, Instruction& instr){
            instr.rs1 = register_extract(lex.arg_list[1]); 
            instr.rs2 = register_extract(lex.arg_list[2]); 
           
            if(label_map.find(lex.arg_list[3]) != label_map.end()){
                int32_t label_loc = this->label_map.at(lex.arg_list[3]); 
                instr.immediate = (label_loc - lex.instr_pos); 
            }
            else{
                instr.immediate = immediate_extract(lex.arg_list[3]); 
            }        
        }

         void parse_u_type(const LexedInstruction& lex, Instruction& instr){
            instr.rd = register_extract(lex.arg_list[1]); 
            instr.immediate = immediate_extract(lex.arg_list[2]); 
        }

        void parse_j_type(const LexedInstruction& lex, Instruction& instr){
            instr.rd = register_extract(lex.arg_list[1]);

            if(label_map.find(lex.arg_list[2]) != label_map.end()){
                int32_t label_loc = this->label_map.at(lex.arg_list[2]); 
                instr.immediate = label_loc - lex.instr_pos; 
            }
            else{
                instr.immediate = immediate_extract(lex.arg_list[3]); 
            }  
        }

        
        void process_dispatcher(const LexedInstruction& lex_instr){
            auto& opcode_str = lex_instr.arg_list.at(0); 
            if(opcode_map.find(opcode_str) != opcode_map.end()){
                auto instr_obj = opcode_map.at(opcode_str); 
                instr_obj.address = lex_instr.instr_pos; 
                switch(instr_obj.opcode){
                    case R_TYPE_OPCODE:
                        parse_r_type(lex_instr, instr_obj); 
                        break; 
                    case I_TYPE_MATH_OPCODE:
                        parse_i_math_type(lex_instr, instr_obj); 
                        break; 
                    case I_TYPE_LOAD_OPCODE: 
                        parse_i_load_type(lex_instr, instr_obj); 
                        break; 
                    case I_TYPE_JALR_OPCODE:
                        parse_i_jalr_type(lex_instr, instr_obj); 
                        break; 
                    case S_TYPE_OPCODE: 
                        parse_s_type(lex_instr, instr_obj); 
                        break; 
                    case SB_TYPE_OPCODE: 
                        parse_sb_type(lex_instr, instr_obj); 
                        break;
                    case U_TYPE_OPCODE:
                        parse_sb_type(lex_instr, instr_obj); 
                        break; 
                    case J_TYPE_OPCODE: 
                        parse_j_type(lex_instr, instr_obj); 
                        break; 
                    case EXIT_SIM_OPCODE:
                        break; 
                    case NOP_OPCODE:
                        break; 
                    default:
                        throw std::invalid_argument("Unsupported opcode of type: " + opcode_str); 
                }

                uint32_t new_instr = 0; 
                encodeInstruction(instr_obj,new_instr); 
                this->instr_structs.push_back(instr_obj); 
                this->instructions.push_back(new_instr);
            }
            else{
                throw std::invalid_argument("Unsupported opcode of type: " + opcode_str); 
            } 
        }


        void read_source(){
            std::string line; 
            std::stringstream word_stream; 
            while(std::getline(source_file, line)){
                std::vector<std::string> arg_list; 
                for(auto c : line){
                    if(c == ',' || std::isspace(c)){
                        if(word_stream.str() != ""){
                            arg_list.push_back(word_stream.str()); 
                            word_stream.str(""); 
                            word_stream.clear();
                        }
                    }
                    else if(c == ':'){
                        this->label_map.emplace(word_stream.str(), lexed_list.size() * sizeof(uint32_t)); 
                        word_stream.str(""); 
                        word_stream.clear();
                        continue;
                    }
                    else if(c == ';'){
                        arg_list.push_back(word_stream.str());
                        word_stream.str("");
                        word_stream.clear(); 
                        if(arg_list.size() != 0){
                            this->lexed_list.push_back({arg_list, lexed_list.size() * sizeof(uint32_t)}); 
                        }
                        arg_list.clear(); 
                    }
                    else{
                        word_stream << (char)std::tolower(c); 
                    }
                }

                word_stream.str(""); 
                word_stream.clear(); 
            }

            for(LexedInstruction& val : lexed_list){
                process_dispatcher(val); 
            }

        }

        std::vector<uint32_t>& get_instructions(){
            return this->instructions; 
        }

        std::vector<Instruction>& get_instruction_structs(){
            return this->instr_structs; 
        }; 
};





