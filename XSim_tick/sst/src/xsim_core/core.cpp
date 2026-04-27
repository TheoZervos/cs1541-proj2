#include <sst/core/sst_config.h>
#include <sst/core/interfaces/stdMem.h>

// #include <sst/core/simulation.h>

#include <sstream>
#include <cstdlib>
#include <fstream>
#include <cmath>

#include <xsim_core/core.hpp>

#include <json/json.h>
#include <json/writer.h>

namespace XSim
{
namespace Core
{



Core::Core(ComponentId_t id, Params& params):
	Component(id)
{

	// verbose is one of the configuration options for this component
	verbose = params.find<uint32_t>("verbose", verbose);

	// clock_frequency is one of the configuration options for this component - parse config file for clock 
	std::string config_path = params.find<std::string>("configuration", ""); //add conditions for no config file?
	std::ifstream config_file(config_path);
	Json::Value config;
	config_file >> config;
	config_file.close();
	clock_frequency = config["clock"].asString();

	//init config FU and RS
	config_int_num = config["integer.number"].asInt();
	config_int_resnum = config["integer.resnumber"].asInt();
	config_mult_num = config["multiplier.number"].asInt();
	config_mult_resnum = config["multiplier.resnumber"].asInt();
	config_div_num = config["divider.number"].asInt();
	config_div_resnum = config["divider.resnumber"].asInt();
	config_ls_num = config["ls.number"].asInt();
	config_ls_resnum = config["ls.resnumber"].asInt();

	// set up statistics tracking
	output_fpath = params.find<std::string>("output", output_fpath);
	stats_json["author"] = "mip132, tnz3";

	// set instruction latencies - redefine
	load_latencies(config);

	// load the program that is to be executed
	load_program(params);

	// Create the SST output with the required verbosity level
	output = new Output("mips_core[@t:@l]: ", verbose, 0, Output::STDOUT);

	// Create a tick function with the frequency specified
	tc = Super::registerClock( clock_frequency, new Clock::Handler2<Core, &Core::tick>(this) );

	output->verbose(CALL_INFO, 1, 0, "Configuring connection to memory...\n");
	// memory_wrapper is used to make write/read requests to the SST simulated memory
	memory_wrapper = loadComponentExtension<MemoryWrapper>(params, output, tc);
	// new MemoryWrapper(*this, params, output, tc);
	output->verbose(CALL_INFO, 1, 0, "Configuration of memory interface completed.\n");

	// SST statistics
	instruction_count = registerStatistic<uint64_t>( "instructions" );

	// tell the simulator not to end without us
	registerAsPrimaryComponent();
	primaryComponentDoNotEndSim();
}

void Core::init(unsigned int phase)
{
	memory_wrapper->init(phase);
}

void Core::init_opcode_types()
{
	opcode_types[ADD] = "integer";
	opcode_types[SUB] = "integer";
	opcode_types[NOR] = "integer";
	opcode_types[AND] = "integer";
	opcode_types[LIS] = "integer";
	opcode_types[LIZ] = "integer";
	opcode_types[LUI] = "integer";
	opcode_types[PUT] = "integer";
	opcode_types[HALT] = "integer";

	opcode_types[DIV] = "divider";
	opcode_types[MOD] = "divider";
	opcode_types[EXP] = "divider";

	opcode_types[MUL] = "multiplier";
	
	opcode_types[LW] = "ls";
	opcode_types[SW] = "ls";
}

void Core::setup()
{
	output->output("Setting up.\n");

	// rename table initialized
	// match config rs pool sizes
    integer_rs.assign(config_int_resnum, {false, false, -1, "integer"});
    multiplier_rs.assign(config_mult_resnum, {false, false, -1, "multiplier"});
    divider_rs.assign(config_div_resnum, {false, false, -1, "divider"});
    ls_rs.assign(config_ls_resnum, {false, false, -1, "ls"});

	// assigning station ids
	for (int i = 0; i < config_int_resnum; i++) integer_rs[i].station_id = i;
	for (int i = 0; i < config_div_resnum; i++) divider_rs[i].station_id = i;
	for (int i = 0; i < config_mult_resnum; i++) multiplier_rs[i].station_id = i;
	for (int i = 0; i < config_ls_resnum; i++) ls_rs[i].station_id = i;

	// initialize all functional units to free
	integer_fu_list.assign(config_int_num, {false, 0, "integer"});
	multiplier_fu_list.assign(config_mult_num, {false, 0, "multiplier"});
    divider_fu_list.assign(config_div_num, {false, 0, "divider"});
    ls_fu_list.assign(config_ls_num, {false, 0, "ls"});

	// initialize opcode types
	init_opcode_types();

	// Setting up output json
	stats_json["cycles"] = 0;
	stats_json["integer"] = Json::arrayValue;
	stats_json["multiplier"] = Json::arrayValue;
	stats_json["divider"] = Json::arrayValue;
	stats_json["ls"] = Json::arrayValue;
	stats_json["reg_reads"] = 0;
	stats_json["stalls"] = 0;

	// Setting up instruction counting vars and storage
	Json::Value nested_obj;
	nested_obj["id"] = 0;
	nested_obj["instructions"] = 0;

	stats_json["integer"].append(nested_obj);
	stats_json["integer"][ADD_ID]["id"] = ADD_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][SUB_ID]["id"] = SUB_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][NOR_ID]["id"] = NOR_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][AND_ID]["id"] = AND_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][LIS_ID]["id"] = LIS_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][LIZ_ID]["id"] = LIZ_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][LUI_ID]["id"] = LUI_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][PUT_ID]["id"] = PUT_ID;
	stats_json["integer"].append(nested_obj);
	stats_json["integer"][HALT_ID]["id"] = HALT_ID;

	stats_json["divider"].append(nested_obj);
	stats_json["divider"][DIV_ID]["id"] = DIV_ID;
	stats_json["divider"].append(nested_obj);
	stats_json["divider"][EXP_ID]["id"] = EXP_ID;
	stats_json["divider"].append(nested_obj);
	stats_json["divider"][MOD_ID]["id"] = MOD_ID;

	stats_json["multiplier"].append(nested_obj);
	stats_json["multiplier"][MUL_ID]["id"] = MUL_ID;

	stats_json["ls"].append(nested_obj);
	stats_json["ls"][LW_ID]["id"] = LW_ID;
	stats_json["ls"].append(nested_obj);
	stats_json["ls"][SW_ID]["id"] = SW_ID;

	std::cout << "========== STARTED PROGRAM ==========" << std::endl;
}

void Core::finish()
{
	// writing json to file
	std::ofstream file(output_fpath);
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "	";
	Json::StreamWriter* jsonWriter = writer.newStreamWriter();
	jsonWriter->write(stats_json, &file);
	file.close();

	std::cout<<"========== FINISHED PROGRAM =========="<<std::endl;
}

/**
 * @brief This function loads the program from the file in parameter: "program"
 */
void Core::load_program(Params &params)
{
	std::string program_path = params.find<std::string>("program","");
	std::ifstream f(program_path);
	if(!f.is_open())
	{
		std::cerr<<"Invalid program file: "<<program_path<<std::endl;
		exit(EXIT_FAILURE);
	}
	for( std::string line; getline( f, line ); )
	{
		std::size_t first_comment=line.find("#");
		std::size_t first_number = line.find_first_of("0123456789ABCDEFabcdef");
		// Ignore comments
		if( first_number<first_comment)
		{
			std::stringstream ss;
			ss << std::hex << line.substr(first_number,4);
			uint16_t instruction;
			ss >> instruction;
			program.push_back(instruction);
		}
	}
	// Print the program for visual inspection
	std::cout<<"Program:"<<std::endl;
	for(uint16_t instruction: program)
	{
		std::cout<<std::hex<<instruction<<std::dec<<std::endl;
	}
}

/**
 * @brief This function loads the latencies from the config file
 */
void Core::load_latencies(Json::Value &config)
{
	// get latencies from json
	int_lat = config["integer.latency"].asUInt();
    mult_lat = config["multiplier.latency"].asUInt();
    div_lat = config["divider.latency"].asUInt();
    ls_lat = config["ls.latency"].asUInt();
}

bool Core::tick(Cycle_t cycle)
{
	std::cout << "Tick" << std::endl;
	// writing back outcomes (execution finished)
	write_registers();

	// executing instructions in functional units
	execute();

	// reading operands
	std::cout << "Read Operands" << std::endl;
	uint16_t next_int_index = read_integer_operands();
	uint16_t next_div_index = read_divider_operands();
	uint16_t next_mult_index = read_multiplier_operands();
	uint16_t next_ls_index = read_ls_operands();

	// assigning reserved instruction to functional unit
	if (next_int_index != 9999) {
		assign_integer_fu(integer_rs[next_int_index]);
	}
	if (next_div_index != 9999) {
		assign_divider_fu(divider_rs[next_div_index]);
	}
	if (next_mult_index != 9999) {
		assign_multiplier_fu(multiplier_rs[next_mult_index]);
	}
	if (next_ls_index != 9999) {
		assign_ls_fu(ls_rs[next_ls_index]);
	}

	// issue inst to reservation stations (stalls inside itself)
	if (!halt_issued && pc < program.size()) { // halt stops all new instructions, flushes pipeline
		issue();
	} else {
		stalled = false; //draining
	}

	if (stalled)
		stats_json["stalls"] = stats_json["stalls"].asUInt() + 1;

	stats_json["cycles"] = stats_json["cycles"].asUInt() + 1;

	if (pc >= program.size() && rs_empty() && fus_idle()) {
		primaryComponentOKToEndSim();
		return true;
	}

	return false;
}

void Core::issue()
{
	std::cout << "Issue" << std::endl;
	uint16_t instruction = program[pc];
	uint16_t opcode = instruction >> 11;
	uint16_t rd, rs, rt, imm8;
	bool is_immediate_load = (opcode == LUI || opcode == LIS || opcode == LIZ);

	res_slot_t *target_res = nullptr;
	int rs_index = -1;

	// halting issued
	if (opcode == HALT) {
		std::cout << "HALT instruction detected. Preventing further issued." << std::endl;
		halt_issued = true;
	}

	// get instruction type
	if (is_immediate_load) {
		std::cout << "Instruction is Immediate!" << std::endl;

		target_res = find_free_rs(integer_rs, rs_index); 
		if (target_res == nullptr) {
			std::cout << "Stalling" << std::endl;
			stalled = true;
			return;
		}

		get_i_fields(instruction, rd, imm8);
		// These instructions don't wait for anyone!
		target_res->op1_ready = true;
		target_res->op1_tag = -1;

		target_res->op2_ready = true;
		target_res->op2_tag = -1;
		
		// Note: You'll likely store the immediate value itself 
		// in a separate 'imm' field in your res_slot_t
		target_res->immediate = imm8; 
	} 
	else {
		get_r_fields(instruction, rd, rs, rt);
		if (opcode_types[opcode] == "integer") {
			std::cout << "Instruction is Integer" << std::endl;
			target_res = find_free_rs(integer_rs, rs_index); 
		} else if (opcode_types[opcode] == "divider") {
			std::cout << "Instruction is Divider" << std::endl;
			target_res = find_free_rs(divider_rs, rs_index);
		} else if (opcode_types[opcode] == "multiplier") {
			std::cout << "Instruction is Multiplier" << std::endl;
			target_res = find_free_rs(multiplier_rs, rs_index);
		} else if (opcode_types[opcode] == "ls") {
			std::cout << "Instruction is Load/Store" << std::endl;
			target_res = find_free_rs(ls_rs, rs_index);
		}

		if (target_res == nullptr) {
			std::cout << "Stalling" << std::endl;
			stalled = true;
			return;
		}

		// source one
		if (operands[rs].ready) {
			target_res->op1_ready = true;
			target_res->op1_tag = -1;
			stats_json["reg_reads"] = stats_json["reg_reads"].asUInt() + 1;
		}
		else
		{
			target_res->op1_ready = false;
			target_res->op1_tag = operands[rs].tag;
			target_res->op1_type = operands[rs].type;
		}

		// source two
		if (operands[rt].ready) {
			target_res->op2_ready = true;
			target_res->op2_tag = -1;
			stats_json["reg_reads"] = stats_json["reg_reads"].asUInt() + 1;
		} else {
			target_res->op2_ready = false;
			target_res->op2_tag = operands[rt].tag;
			target_res->op2_type = operands[rt].type;
		}
	}

	// rename destination
	std::cout << "Renaming Destination" << std::endl;
	target_res->dest = rd;
	operands[rd].ready = false;
	operands[rd].tag = rs_index;
	operands[rd].type = opcode_types[opcode];

	target_res->instruction_id = pc;
	target_res->taken = true;
	stalled = false;

	std::cout << "Moving to Next Instruction" << std::endl;
	pc += 1;
}

bool Core::rs_empty()
{
	// is integer rs empty
	for (auto& res : integer_rs) {
		if (res.taken == true)
			return false;
	}
	// is divider rs empty
	for (auto& res : divider_rs) {
		if (res.taken == true)
			return false;
	}
	// is multiplier rs empty
	for (auto& res : multiplier_rs) {
		if (res.taken == true)
			return false;
	}
	// is ls rs empty
	for (auto& res : ls_rs) {
		if (res.taken == true)
			return false;
	}
	return true;
}

bool Core::fus_idle()
{
	// is integer fus idle
	for (auto& fu : integer_fu_list) {
		if (fu.busy == true)
			return false;
	}
	// is divider fus idle
	for (auto& fu : divider_fu_list) {
		if (fu.busy == true)
			return false;
	}
	// is multiplier fus idle
	for (auto& fu : multiplier_fu_list) {
		if (fu.busy == true)
			return false;
	}
	// is ls fus idle
	for (auto& fu : ls_fu_list) {
		if (fu.busy == true)
			return false;
	}
	return true;
}

res_slot_t* Core::find_free_rs(std::vector<res_slot_t>& rs_list, int& found_index) {
	std::cout << "Finding free reservation station" << std::endl;
	for (size_t i = 0; i < rs_list.size(); ++i) {
        if (!rs_list[i].taken) {
            found_index = i;
			return &rs_list[i];
		}
    }
    return nullptr;
}

void Core::execute()
{
	std::cout << "Execute" << std::endl;
	execute_integer();
	execute_multiplier();
	execute_divider();
	execute_ls();
}

void Core::write_registers()
{
	std::cout << "Writeback" << std::endl;
	writeback_fu_registers(integer_fu_list);
	writeback_fu_registers(divider_fu_list);
	writeback_fu_registers(multiplier_fu_list);
	writeback_fu_registers(ls_fu_list);
}

void Core::writeback_fu_registers(std::vector<functional_unit>& fu_list)
{
	// find completed functional units
	for(auto& fu : fu_list)
	{
		// broadcast to cdb if complete
		std::cout << "CYCLES REMAINING (" << fu.tag << "): " << fu.cycles_remaining << std::endl;
		if (fu.busy && fu.cycles_remaining <= 0)
		{
			update_instruction_count(fu.res.instruction_id);
			if (fu.tag != -1)
			{
				cdb_broadcast(fu.tag, fu.type);
				release_rs_slot(fu.tag, fu.type);
			}
			fu.busy = false;
			fu.tag = -1;
		}
	}
}

void Core::cdb_broadcast(int fu_tag, std::string type) {
    // update the operands
	std::cout << "Broadcasting to CDB: tag: " << fu_tag << " type: " << type << std::endl;
	for (size_t i = 0; i < operands.size(); ++i) {
        if (!operands[i].ready && 
             operands[i].tag == fu_tag && 
             operands[i].type == type) 
        {
            operands[i].ready = true;
            operands[i].tag = -1;
            operands[i].type = "";
        }
    }

	// update operands associated with reservation stations
    update_rs_operands(integer_rs, fu_tag, type);
    update_rs_operands(multiplier_rs, fu_tag, type);
    update_rs_operands(divider_rs, fu_tag, type);
    update_rs_operands(ls_rs, fu_tag, type);
}

void Core::update_rs_operands(std::vector<res_slot_t>& rs_list, int tag, std::string type) {
    std::cout << "Updating Reservation Station Operands: type: " << type << std::endl;
	for (auto &slot : rs_list) {
        if (slot.taken) {
            if (!slot.op1_ready && slot.op1_tag == tag && slot.op1_type == type) {
                slot.op1_ready = true;
            }
            if (!slot.op2_ready && slot.op2_tag == tag && slot.op2_type == type) {
                slot.op2_ready = true;
            }
        }
    }
}

void Core::release_rs_slot(int tag, std::string type)
{
	std::cout << "Releasing Reservation Station Slot " << tag << ":" << type << std::endl;
	if (type == "integer") {
		integer_rs[tag].taken = false;
		integer_rs[tag].dispatched = false;
	} else if (type == "divider") {
		divider_rs[tag].taken = false;
		integer_rs[tag].dispatched = false;
	} else if (type == "multiplier") {
		multiplier_rs[tag].taken = false;
		integer_rs[tag].dispatched = false;
	} else if (type == "ls") {
		ls_rs[tag].taken = false;
		integer_rs[tag].dispatched = false;
	}
	std::cout << "DEBUG: RS " << type << " index " << tag << " IS NOW FREE." << std::endl;
}

void Core::execute_integer()
{
	std::cout << "Executing Integer Functional Units" << std::endl;
	for (auto &fu : integer_fu_list) {
		if (fu.busy) {
			std::cout << "Executed " << fu.tag << ":" << fu.type << std::endl;
			fu.cycles_remaining--;
		}
	}
}

void Core::execute_divider()
{
	std::cout << "Executing Divider Functional Units" << std::endl;
	for (auto &fu : divider_fu_list) {
		if (fu.busy) {
			std::cout << "Executed " << fu.tag << ":" << fu.type << std::endl;
			fu.cycles_remaining--;
		}
	}
}

void Core::execute_multiplier()
{
	std::cout << "Executing Multiplier Functional Units" << std::endl;
	for (auto &fu : multiplier_fu_list) {
		if (fu.busy) {
			std::cout << "Executed " << fu.tag << ":" << fu.type << std::endl;
			fu.cycles_remaining--;
		}
	}
}

void Core::execute_ls()
{
	std::cout << "Executing Load/Store Functional Units" << std::endl;
	for (auto &fu : ls_fu_list) {
		if (fu.busy == true) {
			std::cout << "Executed " << fu.tag << ":" << fu.type << std::endl;
			fu.cycles_remaining--;
		}
	}
}

uint16_t Core::read_integer_operands()
{
	std::cout << "Reading Integer Operands" << std::endl;
	// assigning functional units for integer operand
	int oldest_index = 2147483647;
	uint16_t index = 9999;
	for (uint16_t i = 0; i < integer_rs.size(); ++i)
	{
		const auto &res = integer_rs[i];

		// checking if there is instruction in reservation station
		if (res.taken && !res.dispatched) {
			std::cout << "Integer " << res.station_id << ": op1_ready - " << res.op1_ready << " | op2_ready - " << res.op2_ready << std::endl;
			if (res.op1_ready && res.op2_ready)
			{
				// assigning to new if possible
				if (index == 9999 || res.instruction_id < oldest_index) {
					oldest_index = res.instruction_id;
					index = i;
				}
			}
		}
	}

	return index;
}

uint16_t Core::read_divider_operands()
{
	std::cout << "Reading Divider Operands" << std::endl;
	// assigning functional units for integer operand
	int oldest_index = 2147483647;
	uint16_t index = 9999;
	for (uint16_t i = 0; i < divider_rs.size(); ++i)
	{
		const auto &res = divider_rs[i];

		// checking if there is instruction in reservation station
		if (res.taken && !res.dispatched) {
			if(res.op1_ready && res.op2_ready) {
				// assigning to new if possible
				if (index == 9999 || res.instruction_id < oldest_index) {
					oldest_index = res.instruction_id;
					index = i;
				}
			}
		}
	}

	return index;
}

uint16_t Core::read_multiplier_operands()
{
	std::cout << "Reading Multiplier Operands" << std::endl;
	// assigning functional units for integer operand
	int oldest_index = 2147483647;
	uint16_t index = 9999;
	for (uint16_t i = 0; i < multiplier_rs.size(); ++i)
	{
		const auto &res = multiplier_rs[i];

		// checking if there is instruction in reservation station
		if (res.taken && !res.dispatched) {
			if(res.op1_ready && res.op2_ready) {
				// assigning to new if possible
				if (index == 9999 || res.instruction_id < oldest_index) {
					oldest_index = res.instruction_id;
					index = i;
				}
			}
		}
	}

	return index;
}

uint16_t Core::read_ls_operands()
{
	std::cout << "Reading Load/Store Operands" << std::endl;
	// assigning functional units for integer operand
	int oldest_index = 2147483647;
	uint16_t index = 9999;
	for (uint16_t i = 0; i < ls_rs.size(); ++i)
	{
		const auto &res = ls_rs[i];

		// checking if there is instruction in reservation station
		if (res.taken && !res.dispatched) {
			if(res.op1_ready && res.op2_ready) {
				// assigning to new if possible
				if (index == 9999 || res.instruction_id < oldest_index) {
					oldest_index = res.instruction_id;
					index = i;
				}
			}
		}
	}

	return index;
}

void Core::assign_integer_fu(res_slot_t &res)
{
	std::cout<<"Adding to Integer Functional Unit"<<std::endl;
	for (auto &fu : integer_fu_list) {
		if (!fu.busy) {
			fu.busy = true;
			res.dispatched = true;
			fu.cycles_remaining = int_lat;
			fu.res = res;
			fu.tag = res.station_id;
			break;
		}
	}
}

void Core::assign_divider_fu(res_slot_t &res)
{
	std::cout<<"Adding to Divider Functional Unit"<<std::endl;
	for (auto &fu : divider_fu_list) {
		if (fu.busy == false) {
			fu.busy = true;
			res.dispatched = true;
			fu.cycles_remaining = div_lat;
			fu.res = res;
			fu.tag = res.station_id;
			break;
		}
	}
}

void Core::assign_multiplier_fu(res_slot_t &res)
{
	std::cout<<"Adding to Multiplier Functional Unit"<<std::endl;
	for (auto &fu : multiplier_fu_list) {
		if (fu.busy == false) {
			fu.busy = true;
			res.dispatched = true;
			fu.cycles_remaining = mult_lat;
			fu.res = res;
			fu.tag = res.station_id;
			break;
		}
	}
}

void Core::assign_ls_fu(res_slot_t &res)
{
	std::cout<<"Adding to Load/Store Functional Unit"<<std::endl;
	for (auto &fu : ls_fu_list) {
		if (fu.busy == false) {
			fu.busy = true;
			res.dispatched = true;
			fu.cycles_remaining = ls_lat;
			fu.res = res;
			fu.tag = res.station_id;
			break;
		}
	}
}

void Core::get_r_fields(uint16_t &inst, uint16_t &rd, uint16_t &rs, uint16_t &rt)
{
	// getting register values
	rd = inst >> 8 & 0x07;
	rs = inst >> 5 & 0x07;
	rt = inst >> 2 & 0x07;
}

void Core::get_i_fields(uint16_t &inst, uint16_t &rd, uint16_t &imm8)
{
	rd = inst >> 8 & 0x07;
	imm8 = inst & 0xFF;
}

int Core::low_bits(int num) { return num & 0xFFFF; }

int16_t Core::s_ext(uint8_t imm8)
{
	int16_t extended = 0;
	uint8_t msb = imm8 >> 7 & 0x01;

	// extend with 1s
	if(msb == 1) {
		extended = imm8 | 0xFF00;
		return extended;
	}
	else
	{ // extend with 0s
		extended = imm8;
		return extended;
	}
}

uint16_t Core::concat8bit(uint8_t high_bits, uint8_t low_bits) { return (high_bits << 8) | low_bits; }

void Core::update_instruction_count(uint32_t inst_loc)
{
	// Better be aligned!!
	uint16_t instruction = program[inst_loc];
	uint16_t opcode = instruction >> 11;

	std::cout<<"Running "<<names[opcode]<<std::endl;
	switch (opcode)
	{
		case ADD:
			stats_json["integer"][ADD_ID]["instructions"] = stats_json["integer"][ADD_ID]["instructions"].asInt() + 1;;
			break;

		case SUB:
			stats_json["integer"][SUB_ID]["instructions"] = stats_json["integer"][SUB_ID]["instructions"].asInt() + 1;;
			break;

		case AND:
			stats_json["integer"][AND_ID]["instructions"] = stats_json["integer"][AND_ID]["instructions"].asInt() + 1;;
			break;

		case NOR:
			stats_json["integer"][NOR_ID]["instructions"] = stats_json["integer"][NOR_ID]["instructions"].asInt() + 1;;
			break;

		case DIV:
			stats_json["divider"][DIV_ID]["instructions"] = stats_json["divider"][DIV_ID]["instructions"].asInt() + 1;;
			break;

		case MUL:
			stats_json["multiplier"][MUL_ID]["instructions"] = stats_json["multiplier"][MUL_ID]["instructions"].asInt() + 1;;
			break;

		case MOD:
			stats_json["divider"][MOD_ID]["instructions"] = stats_json["divider"][MOD_ID]["instructions"].asInt() + 1;
			break;

		case EXP:
			stats_json["divider"][EXP_ID]["instructions"] = stats_json["divider"][EXP_ID]["instructions"].asInt() + 1;
			break;

		case LW:
			stats_json["ls"][LW_ID]["instructions"] = stats_json["ls"][LW_ID]["instructions"].asInt() + 1;
			break;

		case SW:
			stats_json["ls"][SW_ID]["instructions"] = stats_json["ls"][SW_ID]["instructions"].asInt() + 1;
			break;

		case HALT:
			std::cout<<"Executing HALT instruction"<<std::endl;
			stats_json["integer"][HALT_ID]["instructions"] = stats_json["integer"][HALT_ID]["instructions"].asInt() + 1;
			break;

		case PUT:
			std::cout<<"Executing PUT instruction"<<std::endl;
			stats_json["integer"][PUT_ID]["instructions"] = stats_json["integer"][PUT_ID]["instructions"].asInt() + 1;
			break;

		case LIZ:
			stats_json["integer"][LIZ_ID]["instructions"] = stats_json["integer"][LIZ_ID]["instructions"].asInt() + 1;
			break;

		case LIS:
			stats_json["integer"][LIS_ID]["instructions"] = stats_json["integer"][LIS_ID]["instructions"].asInt() + 1;
			break;

		case LUI:
			stats_json["integer"][LUI_ID]["instructions"] = stats_json["integer"][LUI_ID]["instructions"].asInt() + 1;
			break;

		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// ~~~~~~~~~~~~ UNEEDED BUT ~~~~~~~~~~~~~
		// ~~~~~~~~~~~ KEPT TO AVOID ~~~~~~~~~~~~
		// ~~~~~~~~~ BREAKING PROGRAM ~~~~~~~~~~~
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		case BP:
			std::cout<<"Executing BP instruction"<<std::endl;
			break;

		case BN:
			std::cout<<"Executing BN instruction"<<std::endl;
			break;

		case BX:
			std::cout<<"Executing BX instruction"<<std::endl;
			break;

		case BZ:
			std::cout<<"Executing BZ instruction"<<std::endl;
			break;

		case J:
			std::cout<<"Executing J instruction"<<std::endl;
			break;
	}
}


}
}