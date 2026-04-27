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

void Core::setup()
{
	output->output("Setting up.\n");

	// rename table initialized
	// match config rs pool sizes
    integer_rs.resize(config_int_resnum);
    multiplier_rs.resize(config_mult_resnum);
    divider_rs.resize(config_div_resnum);
    ls_rs.resize(config_ls_resnum);

	//initialize all functional units to free
    integer_fu_list.resize(config_int_num);
	multiplier_fu_list.resize(config_mult_num);
    divider_fu_list.resize(config_div_num);
    ls_fu_list.resize(config_ls_num);

	// Setting up output json
	stats_json["cycles"] = 0;
	stats_json["integer"] = Json::arrayValue;
	stats_json["multiplier"] = Json::arrayValue;
	stats_json["divider"] = Json::arrayValue;
	stats_json["ls"] = Json::arrayValue;
	stats_json["reg reads"] = 0;
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
	uint32_t int_lat = config["integer.latency"].asUInt();
    uint32_t mul_lat = config["multiplier.latency"].asUInt();
    uint32_t div_lat = config["divider.latency"].asUInt();
    uint32_t ls_lat = config["ls.latency"].asUInt();

    // integer FU opcodes
    latencies[ADD] = int_lat;
	names[ADD] = "add";
	opcode_types[ADD] = "integer";

	latencies[SUB] = int_lat;
	names[SUB] = "sub";
	opcode_types[SUB] = "integer";

    latencies[AND] = int_lat;
	names[AND] = "and";
	opcode_types[AND] = "integer";

    latencies[NOR] = int_lat;
	names[NOR] = "nor";
	opcode_types[NOR] = "integer";

    latencies[LIZ] = int_lat; 
	names[LIZ] = "liz";
	opcode_types[LIZ] = "integer";

    latencies[LIS] = int_lat;
	names[LIS] = "lis";
	opcode_types[LIS] = "integer";

    latencies[LUI] = int_lat;
	names[LUI] = "lui";
	opcode_types[LUI] = "integer";

    latencies[PUT] = int_lat;
	names[PUT] = "put";
	opcode_types[PUT] = "integer";

    latencies[HALT] = int_lat;
	names[HALT] = "halt";
	opcode_types[HALT] = "integer";

    // divider FU opcodes
    latencies[DIV] = div_lat;
	names[DIV] = "div";
	opcode_types[DIV] = "divider";

    latencies[EXP] = div_lat;
	names[EXP] = "exp";
	opcode_types[EXP] = "divider";

    latencies[MOD] = div_lat;
	names[MOD] = "mod";
	opcode_types[MOD] = "divider";

    // multiplier FU opcodes
    latencies[MUL] = mul_lat;
	names[MUL] = "mul";
	opcode_types[MUL] = "multiplier";

    // ls FU opcodes
    latencies[LW] = ls_lat;
	names[LW] = "lw";
	opcode_types[LW] = "ls";

	latencies[SW] = ls_lat;
	names[SW] = "sw";
	opcode_types[SW] = "ls";
}

bool Core::tick(Cycle_t cycle)
{
	std::cout<<"tick"<<std::endl;

	// issue stalls inside itself
	issue();

	// reading operands
	reservation_station_slot_t next_int_inst = read_integer_operands();
	reservation_station_slot_t next_div_inst = read_divider_operands();
	reservation_station_slot_t next_mult_inst = read_multiplier_operands();

	reservation_station_slot_t next_inst = next_int_inst;
	if(next_inst.instruction_id == -1 || next_inst.instruction_id < next_div_inst.instruction_id) {
		next_inst = next_div_inst;
	}
	if(next_inst.instruction_id == -1 || next_inst.instruction_id < next_mult_inst.instruction_id) {
		next_inst = next_mult_inst;
	}

	ls_queue();

	// TODO: Do not add to execution if final id == -1

	// TODO: Execute stage (remember rename table)

	// TODO: Write register stage

	// this flow will likely be completely different
 
	// TODO: Handle issue queue - event flow signals(WR>>RO>>I)

	

	return false;
}

void Core::issue()
{
	std::cout << "Issuing" << std::endl;
	uint16_t instruction = program[pc/2];
	uint16_t opcode = instruction >> 11;
	uint16_t rd, rs, rt, imm8;
	uint16_t op1, op2;

	// getting operands
	if(opcode == LUI || opcode == LIS || opcode == LIZ) {
		get_i_fields(instruction, rd, imm8);
		op1 = rd;
		op2 = rd; // irrelevant second operand
	}
	else
	{
		get_r_fields(instruction, rd, rs, rt);
		op1 = rs;
		op2 = rt;
	}

	// get instruction type
	stalled = true;
	switch (opcode_types[opcode])
	{
	case "integer":
		if (!hold_integer) {
			for (const auto &res : integer_rs)
			{
				if (res.taken == false)
				{
					res.instruction_id = pc;
					res.taken = true;
					res.op1 = op1;
					res.op2 = op2;

					stalled = false;
					break;
				}
			}
		}
		break;

	case "divider":
		if (!hold_divider) {
			for (const auto &res : divider_rs)
			{
				if (res.taken == false)
				{
					res.instruction_id = pc;
					res.taken = true;
					res.op1 = op1;
					res.op2 = op2;

					stalled = false;
					break;
				}
			}
		}
		break;

	case "multiplier":
		if(!hold_multiplier) {
			for (const auto &res : multiplier_rs)
			{
				if (res.taken == false)
				{
					res.instruction_id = pc;
					res.taken = true;
					res.op1 = op1;
					res.op2 = op2;

					stalled = false;
					break;
				}
			}
		}
		break;

	case "ls":
		if(queue_entry_count < (int)ls_rs.size()) {
			ls_reservation_station_slot_t &ls_tail_slot = ls_rs[ls_tail];
			
			// intialize new operation parameters
			ls_tail_slot.taken = true;
			ls_tail_slot.instruction_id = pc;
			ls_tail_slot.load_op = opcode == LW ? true : false;

			if(opcode == LW){ //lw uses one operand
				ls_tail_slot.op1 = rs;
				ls_tail_slot.op2 = 0;
			} else{
				ls_tail_slot.op1 = rt;
				ls_tail_slot.op2 = rs;
			}

			ls_tail_slot.latency = latencies[opcode];
			
			int temp_tail = ls_tail;

			if(opcode == LW) {
				ls_tail_slot.d_register = rd;
				rename_table[rd].ready_for_write = false;
				rename_table[rd].rs_write = temp_tail;
			}

			ls_tail = (ls_tail + 1) % ls_rs.size();
			queue_entry_count++;
			stalled = false;
			
		}
		break;
	}

	// if stalled then pc does not increase until instruction assigned reservation station
	if (stalled){
		stalls++;
		return;
	} 
	// instruction assigned
	next_issue++;
	pc += 2;
}

reservation_station_slot_t Core::read_integer_operands()
{
	std::cout << "Reading Integer Operands" << std::endl;
	// assigning functional units for integer operand
	reservation_station_slot_t next_inst;
	for (const auto &res : integer_rs)
	{
		// checking if there is instruction in reservation station
		if (res.taken == true) {
			if(operands[res.op1].ready == true && operands[res.op2].ready == true) {
				// assigning to new if possible
				if (next_inst.instruction_id != -1 && res.instruction_id < next_inst.instruction_id) {
					next_inst = res;
				}
			}
			else
			{
				continue;
			}
		}
	}

	// TODO: Check if there is an available FU slot and set hold_integer to true if not

	return next_inst;
}

reservation_station_slot_t Core::read_divider_operands()
{
	std::cout << "Reading Divider Operands" << std::endl;
	// assigning functional units for integer operand
	reservation_station_slot_t next_inst;
	for (const auto &res : divider_rs)
	{
		// checking if there is instruction in reservation station
		if (res.taken == true) {
			if(operands[res.op1].ready == true && operands[res.op2].ready == true) {
				// assigning to new if possible
				if (next_inst.instruction_id != -1 && res.instruction_id < next_inst.instruction_id) {
					next_inst = res;
				}
			}
			else
			{
				continue;
			}
		}
	}

	// TODO: Check if there is an available FU slot and set hold_integer to true if not

	return next_inst;
}

reservation_station_slot_t Core::read_multiplier_operands()
{
	std::cout << "Reading Multiplier Operands" << std::endl;
	// assigning functional units for integer operand
	reservation_station_slot_t next_inst;
	for (const auto &res : multiplier_rs)
	{
		// checking if there is instruction in reservation station
		if (res.taken == true) {
			if(operands[res.op1].ready == true && operands[res.op2].ready == true) {
				// assigning to new if possible
				if (next_inst.instruction_id != -1 && res.instruction_id < next_inst.instruction_id) {
					next_inst = res;
				}
			}
			else
			{
				continue;
			}
		}
	}

	// TODO: Check if there is an available FU slot and set hold_integer to true if not

	return next_inst;
}

void Core::fetch_instruction()
{
	// assigning integer functional units
	for()

	// TODO: implement issue addressing -> allocate RS if FU free; stall(add to stall ctr) if not
}

void Core::ls_queue()
{
	// nothing in queue or waiting on cache - do nothing
	if(queue_entry_count == 0 || ls_queue_pending) return;

	// if queue slot empty don't carry out queue ops - safety check
	ls_reservation_station_slot_t &ls_head_slot = ls_rs[ls_head];
	if(!ls_head_slot.taken) return;

	// check if operands ready - if either isn't, wait & exit fn
	bool head_op1_ready = operands[head.op1].ready;
	bool head_op2_ready;
	if (ls_head_slot.load_op){
		head_op2_ready =  operands[ls_head_slot.op2].ready;
	}
	if(!head_op1_ready || !head_op2_ready) return;
	if(ls_head_slot.latency > 0){
		ls_head_slot.latency--;
		return;
	}

	// send operation to cache @ latency completion
	ls_queue_pending = true;
	ls_head_slot.pending = true;
	
	// calculate effective address for lw and sw - mem acesses rs reg
	u_int16_t effective_address;
	if(ls_head_slot.load_op){
		effective_address = registers[ls_head_slot.op1];
	}
	else{
		effective_address = registers[ls_head_slot.op2];
	}

	if(ls_head_slot.load_op){ // LW
		// call mem read & update reg file data
		memory_wrapper -> read(effective_address, [this, ls_head_slot, ls_head_slot.d_register](u_int16_t addr, u_int16_t data)){
			registers[ls_head_slot.d_register] = data;
		}
		
		// cdb broadcast
		operands[ls_head_slot.d_register].ready = true;
		operands[ls_head_slot.d_register].rs_write = -1;

		// update rename table
		if(rename_table[ls_head_slot.d_register].rs_write == ls_head_slot.d_register){
			rename_table[ls_head_slot.d_register].ready_for_write = true;
			rename_table[ls_head_slot.d_register].rs_write = -1;
		}

		// free rs slot and move head in queue
		ls_rs[ls_head_slot.d_register].taken = false;
		ls_rs[ls_head_slot.d_register].pending = false;
		ls_head = (ls_head + 1) % ls_rs.size();
		queue_entry_count--;
		ls_queue_pending = false;
		instruction_completes++;

		// update stats
		stats_json["ls"][0]["instructions"] = stats_json["ls"][0]["instructions"].asInt() + 1;

		// add check for end of program?
		// ...
	} else{ // SW
		// call mem write
		memory_wrapper -> write(effective_address, registers[ls_head_slot.op1], [this](u_int16_t addr)
		{
		// nothing to update in reg file for sw - don't need cdb broadcast or rename table update
		
		// free rs slot and move head in queue
		ls_rs[ls_head_slot.d_register].taken = false;
		ls_rs[ls_head_slot.d_register].pending = false;
		ls_head = (ls_head + 1) % ls_rs.size();
		queue_entry_count--;
		ls_queue_pending = false;
		instruction_completes++;
		
		// update stats
		stats_json["ls"][1]["instructions"] = stats_json["ls"][1]["instructions"].asInt() + 1;
		
		// add check for end of program?
		// ...
		});
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

void Core::execute_instruction()
{
	// TODO: ? - all we do at execute is wait for latency to hold/complete

	// Better be aligned!!
	uint16_t instruction = program[pc/2];
	uint16_t opcode = instruction >> 11;
	uint16_t rd,rs,rt,imm8, result;

	std::cout<<"Running "<<names[opcode]<<std::endl;
	switch (opcode)
	{
		case ADD:
			std::cout<<"Executing ADD instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] + registers[rt];
			pc += 2;
			busy = false;

			stats_json["integer"][ADD_ID]["instructions"] = stats_json["integer"][ADD_ID]["instructions"].asInt() + 1;;
			break;

		case SUB:
			std::cout<<"Executing SUB instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] - registers[rt];
			pc += 2;
			busy = false;

			stats_json["integer"][SUB_ID]["instructions"] = stats_json["integer"][SUB_ID]["instructions"].asInt() + 1;;
			break;

		case AND:
			std::cout<<"Executing AND instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] & registers[rt];
			pc += 2;
			busy = false;
			
			stats_json["integer"][AND_ID]["instructions"] = stats_json["integer"][AND_ID]["instructions"].asInt() + 1;;
			break;

		case NOR:
			std::cout<<"Executing NOR instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = ~(registers[rs] | registers[rt]);
			pc += 2;
			busy = false;
			
			stats_json["integer"][NOR_ID]["instructions"] = stats_json["integer"][NOR_ID]["instructions"].asInt() + 1;;
			break;

		case DIV:
			std::cout<<"Executing DIV instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] / registers[rt];
			pc += 2;
			busy = false;
			
			stats_json["divider"][DIV_ID]["instructions"] = stats_json["divider"][DIV_ID]["instructions"].asInt() + 1;;
			break;

		case MUL:
			std::cout<<"Executing MUL instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = low_bits(registers[rs]*registers[rt]);
			pc += 2;
			busy = false;

			stats_json["multiplier"][MUL_ID]["instructions"] = stats_json["multiplier"][MUL_ID]["instructions"].asInt() + 1;;
			break;

		case MOD:
			std::cout<<"Executing MOD instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] % registers[rt];
			pc += 2;
			busy = false;
			
			stats_json["divider"][MOD_ID]["instructions"] = stats_json["divider"][MOD_ID]["instructions"].asInt() + 1;
			break;

		case EXP:
			std::cout<<"Executing EXP instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			result = registers[rs];

			for (int i = 0; i < registers[rt]-1; i++) {
				result *= registers[rs];
			}

			registers[rd] = result;
			pc += 2;
			busy = false;
			
			stats_json["divider"][EXP_ID]["instructions"] = stats_json["divider"][EXP_ID]["instructions"].asInt() + 1;
			break;

		case LW:
			std::cout<<"Executing LW instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			waiting_memory = true;
			memory_wrapper->read(registers[rs], [this, rd](uint16_t addr, uint16_t data)
			{
				registers[rd]=data;
				pc+=2;
				busy = false;
				waiting_memory = false;
			});
			
			stats_json["ls"][LW_ID]["instructions"] = stats_json["ls"][LW_ID]["instructions"].asInt() + 1;
			break;

		case SW:
			std::cout<<"Executing SW instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			waiting_memory = true;
			memory_wrapper->write(registers[rs], registers[rt], [this](uint16_t addr)
			{
				pc+=2;
				busy = false;
				waiting_memory = false;
			});
			stats_json["ls"][SW_ID]["instructions"] = stats_json["ls"][SW_ID]["instructions"].asInt() + 1;
			break;

		case HALT:
			std::cout<<"Executing HALT instruction"<<std::endl;
			pc+=2;
			primaryComponentOKToEndSim();
			// unregisterExit();
			busy = false;

			stats_json["integer"][HALT_ID]["instructions"] = stats_json["integer"][HALT_ID]["instructions"].asInt() + 1;
			break;

		case PUT:
			std::cout<<"Executing PUT instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			std::cout << "Register " << (int)rs << " = " << registers[rs] << "(unsigned) = " << (int16_t)registers[rs] << "(signed)" << std::endl;
			pc+=2;
			busy = false;

			stats_json["integer"][PUT_ID]["instructions"] = stats_json["integer"][PUT_ID]["instructions"].asInt() + 1;
			break;

		case LIZ:
			std::cout<<"Executing LIZ instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = imm8;
			pc += 2;
			busy = false;

			stats_json["integer"][LIZ_ID]["instructions"] = stats_json["integer"][LIZ_ID]["instructions"].asInt() + 1;
			break;

		case LIS:
			std::cout<<"Executing LIS instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = s_ext(imm8);
			pc += 2;
			busy = false;

			stats_json["integer"][LIS_ID]["instructions"] = stats_json["integer"][LIS_ID]["instructions"].asInt() + 1;
			break;

		case LUI:
			std::cout<<"Executing LUI instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = concat8bit(imm8, (rd & 0xFF));
			pc += 2;
			busy = false;

			stats_json["integer"][LUI_ID]["instructions"] = stats_json["integer"][LUI_ID]["instructions"].asInt() + 1;
			break;

		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// ~~~~~~~~~~~~ UNEEDED BUT ~~~~~~~~~~~~~
		// ~~~~~~~~~~~ KEPT TO AVOID ~~~~~~~~~~~~
		// ~~~~~~~~~ BREAKING PROGRAM ~~~~~~~~~~~
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		case BP:
			std::cout<<"Executing BP instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = ((int16_t) registers[rd]) > 0 ? imm8 << 1 : pc + 2;
			busy = false;
			break;

		case BN:
			std::cout<<"Executing BN instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = ((int16_t) registers[rd]) < 0 ? imm8 << 1 : pc + 2;
			busy = false;
			break;

		case BX:
			std::cout<<"Executing BX instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = registers[rd] != 0 ? imm8 << 1 : pc + 2;
			busy = false;
			break;

		case BZ:
			std::cout<<"Executing BZ instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = registers[rd] == 0 ? imm8 << 1 : pc + 2;
			busy = false;
			break;

		case J:
			std::cout<<"Executing J instruction"<<std::endl;
			uint16_t imm11 = instruction & 0x7FF;
			imm11 = imm11 << 0x01;
			pc += 2;
			busy = false;
			break;
	}
	if(opcode == HALT)
	{
		terminate = true;
	}
}


}
}