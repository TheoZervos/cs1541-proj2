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
	// instruction_count = registerStatistic<uint64_t>( "instructions" );

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
	// output->output("Setting up.\n");

	// rename table initialized
	// match config rs pool sizes
    integer_rs.resize(config_int_resnum);
    multiplier_rs.resize(config_mult_resnum);
    divider_rs.resize(config_div_resnum);
    ls_rs.resize(config_ls_resnum);

    //initialize all fus to free
    free_int_fu.assign(config_int_num, true);
    free_mult_fu.assign(config_mult_num, true);
    free_div_fu.assign(config_div_num, true);
    free_ls_fu.assign(config_ls_num, true);

    //initialize complete instructions to 0
    integer_fu_completes.assign(config_int_num, 0);
    multiplier_fu_completes.assign(config_mult_num, 0);
    divider_fu_completes.assign(config_div_num, 0);
    ls_fu_completes.assign(config_ls_num, 0);

	// Setting up json
	stats_json["cycles"] = 0;
	stats_json["reg reads"] = 0;
	stats_json["stalls"] = 0;

	std::cout << "========== STARTED PROGRAM ==========" << std::endl;
}


void Core::finish()
{
	// saving final register values to json
	

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
    latencies[SUB] = int_lat;
	names[SUB] = "sub";
    latencies[AND] = int_lat;
	names[AND] = "and";
    latencies[NOR] = int_lat;
	names[NOR] = "nor";
    latencies[LIZ] = int_lat; 
	names[LIZ] = "liz";
    latencies[LIS] = int_lat;
	names[LIS] = "lis";
    latencies[LUI] = int_lat;
	names[LUI] = "lui";
    latencies[PUT] = int_lat;
	names[PUT] = "put";
    latencies[HALT] = int_lat;
	names[HALT] = "halt";

    // divider FU opcodes
    latencies[DIV] = div_lat;
	names[DIV] = "div";
    latencies[EXP] = div_lat;
	names[EXP] = "exp";
    latencies[MOD] = div_lat;
	names[MOD] = "mod";

    // multiplier FU opcodes
    latencies[MUL] = mul_lat;
	names[MUL] = "mul";

    // ls FU opcodes
    latencies[LW] = ls_lat;
	names[LW] = "lw";
    latencies[SW] = ls_lat;
	names[SW] = "sw";
}

bool Core::tick(Cycle_t cycle)
{
	// std::cout<<"tick"<<std::endl;
	// stats_json["stats"]["cycles"] = stats_json["stats"]["cycles"].asInt() + 1;
	// if (!busy)
	// {
	// 	fetch_instruction();
	// 	busy = true;
	// }
	// // Block waiting for memory!
	// if(!waiting_memory)
	// {
	// 	if(latency_countdown==0)
	// 	{
	// 		execute_instruction();
	// 		if(instruction_count)
	// 		{
	// 			instruction_count->addData(1);
	// 		}
	// 	}
	// 	else
	// 	{
	// 		latency_countdown--;
	// 	}
	// }
	// return terminate;

	cycle_count = cycle;
 
	// TODO: Handle issue queue - event flow signals(WR>>RO>>I)

	return false;
}

void Core::fetch_instruction()
{
	// Better be aligned!!
	// uint16_t instruction = program[pc/2];
	// uint16_t opcode = instruction >> 11;
	// std::cout<<"Fetched "<<names[opcode]<<std::endl;
	// stats_json["stats"]["instructions"] = stats_json["stats"]["instructions"].asInt() + 1;
	// try
	// {
	// 	latency_countdown = latencies.at(opcode)-1;  //This cycle counts as 1
	// }
	// catch(std::out_of_range &e)
	// {
	// 	std::cerr<<"Unknown instruction: opcode "<<opcode<<std::endl;
	// 	exit(EXIT_FAILURE);
	// }

	// TODO: implement issue addressing -> allocate RS if FU free; stall(add to stall ctr) if not
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
	// uint16_t instruction = program[pc/2];
	// uint16_t opcode = instruction >> 11;
	// uint16_t rd,rs,rt,imm8, result;

	// std::cout<<"Running "<<names[opcode]<<std::endl;
	// switch (opcode)
	// {
	// 	case ADD:
	// 		std::cout<<"Executing ADD instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = registers[rs] + registers[rt];
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["add"] = stats_json["stats"]["add"].asInt() + 1;
	// 		break;

	// 	case SUB:
	// 		std::cout<<"Executing SUB instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = registers[rs] - registers[rt];
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["sub"] = stats_json["stats"]["sub"].asInt() + 1;
	// 		break;

	// 	case AND:
	// 		std::cout<<"Executing AND instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = registers[rs] & registers[rt];
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["and"] = stats_json["stats"]["and"].asInt() + 1;
	// 		break;

	// 	case NOR:
	// 		std::cout<<"Executing NOR instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = ~(registers[rs] | registers[rt]);
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["nor"] = stats_json["stats"]["nor"].asInt() + 1;
	// 		break;

	// 	case DIV:
	// 		std::cout<<"Executing DIV instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = registers[rs] / registers[rt];
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["div"] = stats_json["stats"]["div"].asInt() + 1;
	// 		break;

	// 	case MUL:
	// 		std::cout<<"Executing MUL instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = low_bits(registers[rs]*registers[rt]);
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["mul"] = stats_json["stats"]["mul"].asInt() + 1;
	// 		break;

	// 	case MOD:
	// 		std::cout<<"Executing MOD instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = registers[rs] % registers[rt];
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["mod"] = stats_json["stats"]["mod"].asInt() + 1;
	// 		break;

	// 	case EXP:
	// 		std::cout<<"Executing EXP instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		result = registers[rs];

	// 		for (int i = 0; i < registers[rt]-1; i++) {
	// 			result *= registers[rs];
	// 		}

	// 		registers[rd] = result;
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["exp"] = stats_json["stats"]["exp"].asInt() + 1;
	// 		break;

	// 	case LW:
	// 		std::cout<<"Executing LW instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		waiting_memory = true;
	// 		memory_wrapper->read(registers[rs], [this, rd](uint16_t addr, uint16_t data)
	// 		{
	// 			registers[rd]=data;
	// 			pc+=2;
	// 			busy = false;
	// 			waiting_memory = false;
	// 		});
	// 		stats_json["stats"]["lw"] = stats_json["stats"]["lw"].asInt() + 1;
	// 		break;

	// 	case SW:
	// 		std::cout<<"Executing SW instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		waiting_memory = true;
	// 		memory_wrapper->write(registers[rs], registers[rt], [this](uint16_t addr)
	// 		{
	// 			pc+=2;
	// 			busy = false;
	// 			waiting_memory = false;
	// 		});
	// 		stats_json["stats"]["sw"] = stats_json["stats"]["sw"].asInt() + 1;
	// 		break;

	// 	case JR:
	// 		std::cout<<"Executing JR instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		pc = (registers[rs] & 0xFFFE);
	// 		busy = false;
	// 		stats_json["stats"]["jr"] = stats_json["stats"]["jr"].asInt() + 1;
	// 		break;

	// 	case JALR:
	// 		std::cout<<"Executing JALR instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		registers[rd] = pc+2;
	// 		pc = (registers[rs] & 0xFFFE);
	// 		busy = false;
	// 		stats_json["stats"]["jalr"] = stats_json["stats"]["jalr"].asInt() + 1;
	// 		break;

	// 	case HALT:
	// 		std::cout<<"Executing HALT instruction"<<std::endl;
	// 		pc+=2;
	// 		primaryComponentOKToEndSim();
	// 		// unregisterExit();
	// 		busy = false;
	// 		stats_json["stats"]["halt"] = stats_json["stats"]["halt"].asInt() + 1;
	// 		break;

	// 	case PUT:
	// 		std::cout<<"Executing PUT instruction"<<std::endl;
	// 		get_r_fields(instruction, rd, rs, rt);
	// 		std::cout << "Register " << (int)rs << " = " << registers[rs] << "(unsigned) = " << (int16_t)registers[rs] << "(signed)" << std::endl;
	// 		pc+=2;
	// 		busy = false;
	// 		stats_json["stats"]["put"] = stats_json["stats"]["put"].asInt() + 1;
	// 		break;
		
	// 	case LIZ:
	// 		std::cout<<"Executing LIZ instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		registers[rd] = imm8;
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["liz"] = stats_json["stats"]["liz"].asInt() + 1;
	// 		break;

	// 	case LIS:
	// 		std::cout<<"Executing LIS instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		registers[rd] = s_ext(imm8);
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["lis"] = stats_json["stats"]["lis"].asInt() + 1;
	// 		break;

	// 	case LUI:
	// 		std::cout<<"Executing LUI instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		registers[rd] = concat8bit(imm8, (rd & 0xFF));
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["lui"] = stats_json["stats"]["lui"].asInt() + 1;
	// 		break;

	// 	case BP:
	// 		std::cout<<"Executing BP instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		pc = ((int16_t) registers[rd]) > 0 ? imm8 << 1 : pc + 2;
	// 		busy = false;
	// 		stats_json["stats"]["bp"] = stats_json["stats"]["bp"].asInt() + 1;
	// 		break;

	// 	case BN:
	// 		std::cout<<"Executing BN instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		pc = ((int16_t) registers[rd]) < 0 ? imm8 << 1 : pc + 2;
	// 		busy = false;
	// 		stats_json["stats"]["bn"] = stats_json["stats"]["bn"].asInt() + 1;
	// 		break;

	// 	case BX:
	// 		std::cout<<"Executing BX instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		pc = registers[rd] != 0 ? imm8 << 1 : pc + 2;
	// 		busy = false;
	// 		stats_json["stats"]["bx"] = stats_json["stats"]["bx"].asInt() + 1;
	// 		break;

	// 	case BZ:
	// 		std::cout<<"Executing BZ instruction"<<std::endl;
	// 		get_i_fields(instruction, rd, imm8);
	// 		pc = registers[rd] == 0 ? imm8 << 1 : pc + 2;
	// 		busy = false;
	// 		stats_json["stats"]["bz"] = stats_json["stats"]["bz"].asInt() + 1;
	// 		break;

	// 	case J:
	// 		std::cout<<"Executing J instruction"<<std::endl;
	// 		uint16_t imm11 = instruction & 0x7FF;
	// 		imm11 = imm11 << 0x01;
	// 		pc += 2;
	// 		busy = false;
	// 		stats_json["stats"]["j"] = stats_json["stats"]["j"].asInt() + 1;
	// 		break;
	// }
	// if(opcode == HALT)
	// {
	// 	terminate = true;
	// }
}


}
}
