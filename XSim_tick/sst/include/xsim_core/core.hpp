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

struct rename_table_slot {
	// ready to be written to or not - false if in use by another
	bool ready_for_write{true};
	// which rs is writing to this reg; -1 if not being written to
	int rs_write{-1};
};

struct operand {
	// ready to be read from or not - true if has value & not being written to
	bool ready_for_read{false};
	// which rs is writing to this reg; -1 if not being written to
	int rs_write{-1};
};

struct reservation_station_slot {
	// rs slot taken y/n
	bool busy{false};
	// curr instruction id from pc
	int instruction_id{-1};
	operand op1;
	operand op2;
};

class Core: public SST::Component
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
		// cycle count
		u_int64_t cycle_count{0};
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
		std::array<rename_table_slot, 8> rename_table;
		std::vector<reservation_station_slot> integer_rs;
		std::vector<reservation_station_slot> multiplier_rs;
		std::vector<reservation_station_slot> divider_rs;
		std::vector<reservation_station_slot> ls_rs; 
		// fu free tracking by type
		std::vector<bool> free_int_fu;
		std::vector<bool> free_mult_fu;
		std::vector<bool> free_div_fu;
		// add ls one?
		std::vector<bool> free_ls_fu; //may change later - queue

		// completed instruction count by type & fu
		std::vector<int> integer_fu_completes;
		std::vector<int> multiplier_fu_completes;
		std::vector<int> divider_fu_completes;
		std::vector<int> ls_fu_completes;
		// initialize config default fu count and res stations
		int config_int_num{0}, config_int_resnum{0}, config_mult_num{0}, config_mult_resnum{0}, config_div_num{0}, config_div_resnum{0}, config_ls_num{0}, config_ls_resnum{0};		

		/** CPU state **/
		// The program counter
		uint32_t pc{0};
		// Registers
		std::array<uint16_t,8> registers;
		// Busy processing instruction
		bool busy{false};
		// Busy processing instruction
		int latency_countdown{0};
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

		/** Functions to process events **/
		// Fetch a new instruction
		void fetch_instruction();
		// Execute a new instruction
		void execute_instruction();

		// Statistics definitions
		Statistics<uint64_t> *instruction_count;

		// TimeConverter -> memory needs this
		TimeConverter* tc{nullptr};
};

}
}