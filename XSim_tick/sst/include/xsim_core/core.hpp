#pragma once


#include <sst/core/component.h>
// #include <sst/core/elementinfo.h>
// SST datatypes
#include <sst/core/sst_types.h>
// SST output to print information
#include <sst/core/output.h>
// SST interface for memory
#include <sst/core/interfaces/stdMem.h>
// SST statistics
#include <sst/core/statapi/stataccumulator.h>

#include <xsim_core/memory_wrapper.hpp>
#include <xsim_core/opcodes.hpp>
#include <json/json.h>

namespace XSim
{
namespace Core
{

enum class functional_unit_type {INTEGER, MULTIPLIER, DIVIDER, LS};

struct operand {
	// ready to be read from or not - true if has value & not being written to
	bool ready{true};
	int tag{-1};
	std::string type{"none"};
};

struct res_slot_t {
	// rs slot taken y/n
	bool taken{false};
	bool dispatched{false};
	// curr instruction id from pc
	int instruction_id{-1};
	// type of reservation station
	std::string type;
	// station tag
	int station_id{-1};
	// operand 1
	uint16_t op1;
	bool op1_ready{false};
	int op1_tag{-1};
	std::string op1_type;
	// operand 2
	uint16_t op2;
	bool op2_ready{false};
	int op2_tag{-1};
	std::string op2_type;
	// destination register
	uint32_t immediate{0};
	uint16_t dest;
	// for cache
	bool load_op{false};
	u_int32_t latency{0};
	// operation in cache/memory
	bool pending{false};
};

struct functional_unit {
	bool busy{false};
	uint16_t cycles_remaining{0};
	std::string type;
	int tag{-1};
	res_slot_t res;
};

class Core : public SST::Component
{
	public:
		// SST registration -- See https://sst-simulator.org/sst-docs/docs/guides/dev/devtutorial
		// SST example CPU with memory -- See https://github.com/sstsimulator/sst-elements/blob/master/src/sst/elements/memHierarchy/testcpu/scratchCPU.h
		SST_ELI_REGISTER_COMPONENT(
			Core,
			"XSim",
			"Core",
			SST_ELI_ELEMENT_VERSION(1,0,0),
			"Simple MIPS-based simulator",
			COMPONENT_CATEGORY_PROCESSOR
		)

		SST_ELI_DOCUMENT_PARAMS(
			{ "verbose", "(uint) Verbosity for debugging. Increased numbers for increased verbosity.", "0" },
			{ "program", "(infile) Path to program to be executed by the simulator", "REQUIRED"},
			{ "configuration", "(infile) Path to JSON config file", "REQUIRED" }, 
			{ "output", "(outfile) Path to the file that will store program statistics in JSON format", "statistics.json"},
		)

		// Statistics for our component
		SST_ELI_DOCUMENT_STATISTICS(
			{ "instructions", "Number of instructions executed", "", 0 }
		)

		// This is used to connect the memory interface, thus we don't need to implement one!
		SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( {"memory", "Interface to memory (e.g., caches)", "SST::Interfaces::StandardMem"} )

	private:
		using Super=SST::Component;
		template<typename Type> using Statistics=SST::Statistics::Statistic<Type>;
		using Cycle_t=SST::Cycle_t;
		using StandardMem=SST::Interfaces::StandardMem;
		using Output=SST::Output;
		using ComponentId_t=SST::ComponentId_t;
		using Params=SST::Params;
		using Clock=SST::Clock;
		using TimeConverter = ::SST::TimeConverter;

	public:
		/** These functions are defined by SST **/
		Core(ComponentId_t id, Params& params);
		virtual void init(unsigned int phase) override final;
		virtual void setup() override final;
		virtual void finish() override final;
		bool tick(Cycle_t cycle);

	private:
		/** For completeness **/
		Core();
		Core(const Core& params);
		Core operator=(const Core& params);
		virtual ~Core() = default;

	protected:
		// Method to load the program from a file
		void load_program(Params &params);
		// Method to load instruction latencies
		void load_latencies(Json::Value &config);

	private:
		/** Parameters **/
		// Prints verbose information
		int verbose{0};
		// The core clock frequency
		std::string clock_frequency{"1GHz"};
		// The output file
		std::string output_fpath{"statistics.json"};
		Json::Value stats_json;
		// The vector with the program instructions
		std::vector<uint16_t> program;
		// The map with instruction latencies
		std::map<uint32_t, uint32_t> latencies;
		// The map with instruction names (for printing)
		std::map<uint32_t, std::string> names;

		/** P2 simulation vars **/
		//next issue number
		int next_issue{0};
		// completed instruction count - inc @ WR
		int instruction_completes{0};
		// reg read count
		u_int64_t reg_reads{0};
		// stalls count
		u_int64_t stalls{0};
		// The map with opcode types
		std::map<uint32_t, std::string> opcode_types;
		// The map with the number of times instruction is called
		std::map<uint32_t, uint32_t> int_count;
		std::map<uint32_t, uint32_t> mul_count;
		std::map<uint32_t, uint32_t> div_count;
		std::map<uint32_t, uint32_t> ls_count;

		// res stations by type - size parsed from config
		std::vector<res_slot_t> integer_rs;
		std::vector<res_slot_t> multiplier_rs;
		std::vector<res_slot_t> divider_rs;
		std::vector<res_slot_t> ls_rs; 
		// fu free tracking by type
		std::vector<functional_unit> integer_fu_list;
		std::vector<functional_unit> multiplier_fu_list;
		std::vector<functional_unit> divider_fu_list;
		// add ls one?
		std::vector<functional_unit> ls_fu_list; //may change later - queue

		// halt issued flag
		bool halt_issued{false};

		// initialize config default fu count and res stations
		int config_int_num{0}, config_int_resnum{0}, config_mult_num{0}, config_mult_resnum{0}, config_div_num{0}, config_div_resnum{0}, config_ls_num{0}, config_ls_resnum{0};		

		/** CPU state **/
		// The program counter
		uint32_t pc{0};
		uint32_t stalled_count{0};
		uint32_t cycle_count{0};
		// Registers
		std::array<uint16_t,8> registers;
		std::array<operand, 8> operands;
		// Busy processing instruction
		bool busy{false};
		bool stalled{false};
		// Busy processing instruction
		uint32_t int_lat;
		uint32_t mult_lat;
		uint32_t div_lat;
		uint32_t ls_lat;
		// Waiting for memory return
		bool waiting_memory{false};
		// Finished simulation
		bool terminate{false};

		// SST managed output
		Output *output{nullptr};

		// Wrapper to simplify memory access with SST's memory interface
		MemoryWrapper *memory_wrapper;

		//** Helpers for instruction execution **/
		// get fields for r-type instructions
		void get_r_fields(uint16_t &inst, uint16_t &rd, uint16_t &rs, uint16_t &rt);
		// get fields for i-type instructions
		void get_i_fields(uint16_t &inst, uint16_t &rd, uint16_t &imm8);
		// return the low buts of an int
		int low_bits(int num);
		// return the 16-bit sign extended result of an 8-bit immediate
		int16_t s_ext(uint8_t imm8);
		// concatenate two 8-bit unsigned ints
		uint16_t concat8bit(uint8_t high_bits, uint8_t low_bits);

		void init_opcode_types();

		/** Functions to process events **/
		// Issuue a new instruction
		void issue();
		res_slot_t *find_free_rs(std::vector<res_slot_t> &rs_list, int &found_index);
		// Read operand for instruction in reservation station
		uint16_t read_integer_operands();
		uint16_t read_divider_operands();
		uint16_t read_multiplier_operands();
		uint16_t read_ls_operands();
		// Assigns instruction from reservation station to functional unit
		void assign_integer_fu(res_slot_t &res);
		void assign_divider_fu(res_slot_t &res);
		void assign_multiplier_fu(res_slot_t &res);
		void assign_ls_fu(res_slot_t &res);
		// Execute the instructions in functional units
		void execute();
		void execute_integer();
		void execute_divider();
		void execute_multiplier();
		void execute_ls();
		// Write results to the register
		void write_registers();
		void writeback_fu_registers(std::vector<functional_unit>& fu_list);
		void cdb_broadcast(int fu_tag, std::string type);
		void update_rs_operands(std::vector<res_slot_t> &rs_list, int tag, std::string type);
		void release_rs_slot(int tag, std::string type);
		// Update instruction execution count
		void update_instruction_count(uint32_t inst_loc);
		// Check if reservation stations are empty
		bool rs_empty();
		bool fus_idle();

		/** LS Queue Trackers */
		int ls_head{0};
		int ls_tail{0};
		// current # of entries in queue
		int queue_entry_count{0};
		// true if operation being sent to mem/DRAM - only allowed for one op in queue at a time 
		bool ls_queue_pending{false};
		// ls queue handler
		void ls_queue();
		void cache_issue(uint16_t opcode, uint16_t rs, uint16_t rt, uint16_t rd, int slot_index);

		// Statistics definitions
		Statistics<uint64_t> *instruction_count;

		// TimeConverter -> memory needs this
		TimeConverter* tc{nullptr};
};

}
}