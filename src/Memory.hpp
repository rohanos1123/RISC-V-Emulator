#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream> 
#include <cstdint>
#include <stdexcept>
#include <vector> 
#include <bit> 
#include <memory>

using memaddress_t = uint64_t;
using diskaddress_t = uint16_t;  
using registerint_t  = uint64_t; 

#define PAGE_SIZE 256
#define MEMORY_SIZE 4096
#define DISK_SIZE 8192
#define DISK_SECTOR_SIZE 512
#define DISK_MEM_BUS_WIDTH 64
#define CACHE_LINE_SZ 64
#define REGISTER_COUNT 32

class Memory; 
class Disk;
class PhysicalPage; 
class PhysicalMemory; 
class MemDiskDMA; 



struct CacheLine{
    bool dirty = false; 
    bool valid = false; 
    int last_access = 0; 
    uint64_t tag; 
    std::vector<uint8_t> data;
    uint16_t size; 

    CacheLine()
    :data(CACHE_LINE_SZ), size(CACHE_LINE_SZ)
    {}
}; 

class Memory{
    std::string label; 

    public: 
        Memory(const std::string& label) : label(std::move(label))
        {}

        size_t penalty = 0; 
        virtual void mem_read_reg(const uint64_t &read_address, const uint8_t& capture_sz, uint64_t& bus) = 0;
        virtual void mem_write_reg(const uint64_t &write_address, const uint8_t& capture_sz, const uint64_t& bus) = 0;

        virtual void read_cache_line(const uint64_t& read_address, CacheLine& cache_ln) = 0; 
        virtual void write_cache_line(const uint64_t& write_address, const CacheLine& cache_ln) = 0; 
};


class Disk{
    friend MemDiskDMA; 

    private:
        std::fstream mounted_file;
        uint64_t curr_sector;  

    public: 
        Disk(std::string& filename)
        : mounted_file(filename) 
        {
            if(!mounted_file.is_open()){
                throw std::runtime_error("Failed to mount file: " + filename + " as disk!"); 
            }
        } 

        void disk_seek(uint64_t seek_sector){
            mounted_file.seekg(seek_sector * DISK_SECTOR_SIZE); 
            curr_sector = seek_sector; 
        }; 

        uint64_t get_curr_sector(){
            return curr_sector;    
        }; 
}; 



class PhysicalMemory : public Memory{
    friend MemDiskDMA; 

    private: 
        std::vector<uint8_t> memory; 

    public: 
        PhysicalMemory(const std::string& label)
        :memory(MEMORY_SIZE), 
        Memory(label)
        {} 

        void mem_write_reg(const uint64_t &phy_addr, const uint8_t &size, const uint64_t& reg) override{
            if(size > sizeof(registerint_t)) {throw std::invalid_argument("Attempting to write more than 8 bytes into a register");}
            std::memcpy(memory.data() + phy_addr, (char*)(&reg), size);  
        }   
        
        void mem_read_reg(const uint64_t &phy_addr, const uint8_t &size, uint64_t& reg) override{
            if(size > sizeof(registerint_t)) {throw std::invalid_argument("Attempting to write more than 8 bytes into a register");}
            std::memcpy((char* )(&reg), memory.data() + phy_addr, size); 
        }   

        void read_cache_line(const uint64_t &phy_addr, CacheLine& ln) override{
            std::memcpy( ln.data.data(), memory.data() + phy_addr, CACHE_LINE_SZ); 
        }

        void write_cache_line(const uint64_t &phy_addr, const CacheLine& ln) override{
            throw std::runtime_error("Physical Memory Write ");
        }

        void write_buffer(char* buffer, const uint64_t &address, size_t write_sz){
            std::memcpy(memory.data() + address, buffer, write_sz); 
        }

};

/*
    DMA Simulator Between Object and Disk
*/

class MemDiskDMA{
    private: 
        Disk& disk; 
        PhysicalMemory& memory; 
        std::array<uint8_t, DISK_MEM_BUS_WIDTH> loaded_data; 

    public: 
        MemDiskDMA(Disk& dsk, PhysicalMemory& phymem)
        : disk(dsk), memory(phymem) 
        {} 

        void seek_and_read_sector(diskaddress_t targ_sector, memaddress_t memory_address){
            disk.disk_seek(targ_sector); 
            for(int i = 0; i < (int)DISK_SECTOR_SIZE/DISK_MEM_BUS_WIDTH; i++){
                auto mem_start = memory.memory.data() + (memory_address * sizeof(uint8_t));
                disk.mounted_file.read((char*)mem_start, DISK_MEM_BUS_WIDTH);  
                mem_start += DISK_MEM_BUS_WIDTH; 
            }
        }
}; 


/*
    Caches are Write-Through, meaning that they forward any writes
    to the lower memory

*/
template <size_t size, size_t set_size>  
class Cache : public Memory{

    private:  
        std::shared_ptr<Memory> lower_mem; 
        std::string label; 

        uint64_t access_counter = 0; 
        const size_t set_count = (int)size/(set_size * CACHE_LINE_SZ);

        const memaddress_t offset_mask = CACHE_LINE_SZ - 1; 
        
        const uint8_t index_rs = std::bit_width(offset_mask);  
        const memaddress_t index_mask = (set_count - 1) << (index_rs); 

        const uint8_t tag_rs = (std::bit_width(set_count) - 1) + index_rs; 
        const memaddress_t tag_mask =  ~(offset_mask | index_mask); 
        
        std::vector<CacheLine> data; 
        
    public: 
        Cache(std::shared_ptr<Memory> fall_through, const std::string& label)
        :lower_mem(fall_through),
        Memory(label)
        { 
            for(int i = 0; i < (int)size/CACHE_LINE_SZ; i++){
                data.push_back(CacheLine()); 
            }
        }

        int get_cache_line(const memaddress_t &phy_read_address){
            int set_index = ((phy_read_address & index_mask) >> index_rs); 
            int tag = (phy_read_address & tag_mask) >> tag_rs; 

            for(int i = 0 ; i < set_size; i++){
                auto index = (set_index * set_size) + i; 
                auto& candidate = data.at(index); 
                if((candidate.tag == tag) && candidate.valid){
                    candidate.last_access = access_counter++; 
                    return index; 
                }
            }

            return -1; 
        }

        int lru_evict_from_set(memaddress_t phy_addr){
            int set_index = (phy_addr & index_mask) >> index_rs; 
            uint32_t min_access = 0xFFFFFFFF; 
            uint32_t set_lru = 0; 

            for(int i = 0; i < set_size; i++){
                int curr_index = (set_index * set_size) + i; 
                auto& cal = data.at(curr_index);

                if(!cal.valid){
                    return curr_index; 
                }

                if(cal.last_access < min_access){
                    min_access = cal.last_access; 
                    set_lru = curr_index; 
                }
            }

            return set_lru; 
        }


        CacheLine& receive_cache_line(const memaddress_t &phy_addr){
            int res = get_cache_line(phy_addr); 
            int set = (phy_addr & index_mask) >> index_rs;
            int request_addr = std::floor(phy_addr/CACHE_LINE_SZ) * CACHE_LINE_SZ;  
        
            if(res == -1){
                res = lru_evict_from_set(phy_addr); 
                auto& cl = data.at(res); 
                lower_mem->read_cache_line(request_addr, cl); 
            }

            auto& cl = this->data.at(res); 
            cl.valid = true; 
            cl.last_access = access_counter; 
            cl.tag = (tag_mask & request_addr) >> tag_rs; 
            return cl; 
        }


        void mem_read_reg(const memaddress_t &phy_read_address, const uint8_t& capture_sz, uint64_t& bus) override{
            if(capture_sz > sizeof(registerint_t)) {throw std::invalid_argument("Attempting to read more than 8 bytes into a register");}
            int targ_index = get_cache_line(phy_read_address);
            int line_offset = phy_read_address & offset_mask; 
            if(targ_index != -1){
                //std::cout<<"Read Cache hit from"<<label<<" for address "<<phy_read_address<<std::endl; 
                CacheLine& cl = data.at(targ_index); 
                std::memcpy((char*)(&bus), cl.data.data() + line_offset, capture_sz);
            }
            else{
                //std::cout<<"Read Cache miss from"<<label<<" for address "<<phy_read_address<<std::endl; 
                auto& filled = receive_cache_line(phy_read_address); 
                std::memcpy((char*)(&bus), filled.data.data() + line_offset, capture_sz); 
            }
        }

        void mem_write_reg(const memaddress_t &phy_write_address, const uint8_t& capture_sz, const uint64_t& bus) override{
            if(capture_sz > sizeof(registerint_t)) {throw std::invalid_argument("Attempting to write more than 8 bytes into a register");}
            int targ_index = get_cache_line(phy_write_address); 
            int line_offset = phy_write_address & offset_mask; 
            if(targ_index != -1){
                //std::cout<<"Write Cache hit from"<<label<<" for address "<<phy_write_address<<std::endl; 
                CacheLine& cl = data.at(targ_index); 
                cl.dirty = true; 
                std::memcpy(cl.data.data() + line_offset, (char*)(&bus), capture_sz);
            }
            else{
                //std::cout<<"Write Cache miss from"<<label<<" for address "<<phy_write_address<<std::endl; 
                auto& filled = receive_cache_line(phy_write_address); 
                filled.dirty = true;  
                std::memcpy(filled.data.data() + line_offset, (char*)(&bus),capture_sz); 
            }

            // Write Propogated Immediately in a write-through cache
            lower_mem->mem_write_reg(phy_write_address, capture_sz, bus); 
        } 

        void read_cache_line(const memaddress_t &phy_addr, CacheLine& ln) override{
            auto& cl = receive_cache_line(phy_addr); 
            cl.valid = true; 
            cl.last_access = access_counter; 
            cl.tag = (tag_mask & phy_addr) >> tag_rs; 
            std::memcpy(ln.data.data(), cl.data.data(), CACHE_LINE_SZ);    
        }

        void write_cache_line(const uint64_t &phy_addr, const CacheLine& ln) override{
            throw std::runtime_error("Not yet implemented "); 
        }

}; 


template <size_t count> 
class Register{
    private:
        std::array<uint64_t, count> register_data{}; 

    public: 
       
        uint64_t& operator[](uint8_t index){
            return register_data.at(index); 
        }
}; 
