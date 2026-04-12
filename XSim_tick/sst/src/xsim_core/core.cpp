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

	// clock_frequency is one of the configuration options for this component
	clock_frequency=params.find<std::string>("clock_frequency",clock_frequency);
	this->registerTimeBase(clock_frequency, true );

	// set up statistics tracking
	output_fpath = params.find<std::string>("output", output_fpath);
	stats_json["author"] = "tnz3";

	// set instruction latencies
	load_latencies(params);

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

	// starting all registers at 0
	registers[0] = 0;
	registers[1] = 0;
	registers[2] = 0;
	registers[3] = 0;
	registers[4] = 0;
	registers[5] = 0;
	registers[6] = 0;
	registers[7] = 0;


	// Setting up json
	stats_json["stats"]["add"] = 0;
	stats_json["stats"]["sub"] = 0;
	stats_json["stats"]["and"] = 0;
	stats_json["stats"]["nor"] = 0;
	stats_json["stats"]["div"] = 0;
	stats_json["stats"]["mul"] = 0;
	stats_json["stats"]["mod"] = 0;
	stats_json["stats"]["exp"] = 0;
	stats_json["stats"]["lw"] = 0;
	stats_json["stats"]["sw"] = 0;
	stats_json["stats"]["liz"] = 0;
	stats_json["stats"]["lis"] = 0;
	stats_json["stats"]["lui"] = 0;
	stats_json["stats"]["bp"] = 0;
	stats_json["stats"]["bn"] = 0;
	stats_json["stats"]["bx"] = 0;
	stats_json["stats"]["bz"] = 0;
	stats_json["stats"]["jr"] = 0;
	stats_json["stats"]["jalr"] = 0;
	stats_json["stats"]["j"] = 0;
	stats_json["stats"]["halt"] = 0;
	stats_json["stats"]["put"] = 0;
	stats_json["stats"]["instructions"] = 0;
	stats_json["stats"]["cycles"] = 0;

	std::cout << "========== STARTED PROGRAM ==========" << std::endl;
}


void Core::finish()
{
	// saving final register values to json
	stats_json["registers"]["r0"] = registers[0];
	stats_json["registers"]["r1"] = registers[1];
	stats_json["registers"]["r2"] = registers[2];
	stats_json["registers"]["r3"] = registers[3];
	stats_json["registers"]["r4"] = registers[4];
	stats_json["registers"]["r5"] = registers[5];
	stats_json["registers"]["r6"] = registers[6];
	stats_json["registers"]["r7"] = registers[7];

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
 * @brief This function loads the latencies from the parameters
 */
void Core::load_latencies(Params &params)
{
	// r-type instruction latencies
	latencies.insert({ADD, params.find<uint32_t>("add", 1)});
	names.insert({ADD, "add"});
	latencies.insert({SUB, params.find<uint32_t>("sub", 1)});
	names.insert({SUB, "sub"});
	latencies.insert({AND, params.find<uint32_t>("and", 1)});
	names.insert({AND, "and"});
	latencies.insert({NOR, params.find<uint32_t>("nor", 1)});
	names.insert({NOR, "nor"});
	latencies.insert({DIV, params.find<uint32_t>("div", 1)});
	names.insert({DIV, "div"});
	latencies.insert({MUL, params.find<uint32_t>("mul", 1)});
	names.insert({MUL, "mul"});
	latencies.insert({MOD, params.find<uint32_t>("mod", 1)});
	names.insert({MOD, "mod"});
	latencies.insert({EXP, params.find<uint32_t>("exp", 1)});
	names.insert({EXP, "exp"});
	latencies.insert({LW, params.find<uint32_t>("lw", 1)});
	names.insert({LW, "lw"});
	latencies.insert({SW, params.find<uint32_t>("sw", 1)});
	names.insert({SW, "sw"});
	latencies.insert({JR, params.find<uint32_t>("jr", 1)});
	names.insert({JR, "jr"});
	latencies.insert({JALR, params.find<uint32_t>("jalr", 1)});
	names.insert({JALR, "jalr"});
	latencies.insert({HALT, params.find<uint32_t>("halt", 1)});
	names.insert({HALT, "halt"});
	latencies.insert({PUT, params.find<uint32_t>("put", 1)});
	names.insert({PUT, "put"});

	// i-type instruction latencies
	latencies.insert({LIZ, params.find<uint32_t>("liz", 1)});
	names.insert({LIZ, "liz"});
	latencies.insert({LIS, params.find<uint32_t>("lis", 1)});
	names.insert({LIS, "lis"});
	latencies.insert({LUI, params.find<uint32_t>("lui", 1)});
	names.insert({LUI, "lui"});
	latencies.insert({BP, params.find<uint32_t>("bp", 1)});
	names.insert({BP, "bp"});
	latencies.insert({BN, params.find<uint32_t>("bn", 1)});
	names.insert({BN, "bn"});
	latencies.insert({BX, params.find<uint32_t>("bx", 1)});
	names.insert({BX, "bx"});
	latencies.insert({BZ, params.find<uint32_t>("bz", 1)});
	names.insert({BZ, "bz"});

	// ix-type instruction latencies
	latencies.insert({J, params.find<uint32_t>("j", 1)});
	names.insert({J, "j"});
}

bool Core::tick(Cycle_t cycle)
{
	std::cout<<"tick"<<std::endl;
	stats_json["stats"]["cycles"] = stats_json["stats"]["cycles"].asInt() + 1;
	if (!busy)
	{
		fetch_instruction();
		busy = true;
	}
	// Block waiting for memory!
	if(!waiting_memory)
	{
		if(latency_countdown==0)
		{
			execute_instruction();
			if(instruction_count)
			{
				instruction_count->addData(1);
			}
		}
		else
		{
			latency_countdown--;
		}
	}
	return terminate;
}

void Core::fetch_instruction()
{
	// Better be aligned!!
	uint16_t instruction = program[pc/2];
	uint16_t opcode = instruction >> 11;
	std::cout<<"Fetched "<<names[opcode]<<std::endl;
	stats_json["stats"]["instructions"] = stats_json["stats"]["instructions"].asInt() + 1;
	try
	{
		latency_countdown = latencies.at(opcode)-1;  //This cycle counts as 1
	}
	catch(std::out_of_range &e)
	{
		std::cerr<<"Unknown instruction: opcode "<<opcode<<std::endl;
		exit(EXIT_FAILURE);
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
			stats_json["stats"]["add"] = stats_json["stats"]["add"].asInt() + 1;
			break;

		case SUB:
			std::cout<<"Executing SUB instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] - registers[rt];
			pc += 2;
			busy = false;
			stats_json["stats"]["sub"] = stats_json["stats"]["sub"].asInt() + 1;
			break;

		case AND:
			std::cout<<"Executing AND instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] & registers[rt];
			pc += 2;
			busy = false;
			stats_json["stats"]["and"] = stats_json["stats"]["and"].asInt() + 1;
			break;

		case NOR:
			std::cout<<"Executing NOR instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = ~(registers[rs] | registers[rt]);
			pc += 2;
			busy = false;
			stats_json["stats"]["nor"] = stats_json["stats"]["nor"].asInt() + 1;
			break;

		case DIV:
			std::cout<<"Executing DIV instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] / registers[rt];
			pc += 2;
			busy = false;
			stats_json["stats"]["div"] = stats_json["stats"]["div"].asInt() + 1;
			break;

		case MUL:
			std::cout<<"Executing MUL instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = low_bits(registers[rs]*registers[rt]);
			pc += 2;
			busy = false;
			stats_json["stats"]["mul"] = stats_json["stats"]["mul"].asInt() + 1;
			break;

		case MOD:
			std::cout<<"Executing MOD instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = registers[rs] % registers[rt];
			pc += 2;
			busy = false;
			stats_json["stats"]["mod"] = stats_json["stats"]["mod"].asInt() + 1;
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
			stats_json["stats"]["exp"] = stats_json["stats"]["exp"].asInt() + 1;
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
			stats_json["stats"]["lw"] = stats_json["stats"]["lw"].asInt() + 1;
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
			stats_json["stats"]["sw"] = stats_json["stats"]["sw"].asInt() + 1;
			break;

		case JR:
			std::cout<<"Executing JR instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			pc = (registers[rs] & 0xFFFE);
			busy = false;
			stats_json["stats"]["jr"] = stats_json["stats"]["jr"].asInt() + 1;
			break;

		case JALR:
			std::cout<<"Executing JALR instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			registers[rd] = pc+2;
			pc = (registers[rs] & 0xFFFE);
			busy = false;
			stats_json["stats"]["jalr"] = stats_json["stats"]["jalr"].asInt() + 1;
			break;

		case HALT:
			std::cout<<"Executing HALT instruction"<<std::endl;
			pc+=2;
			primaryComponentOKToEndSim();
			// unregisterExit();
			busy = false;
			stats_json["stats"]["halt"] = stats_json["stats"]["halt"].asInt() + 1;
			break;

		case PUT:
			std::cout<<"Executing PUT instruction"<<std::endl;
			get_r_fields(instruction, rd, rs, rt);
			std::cout << "Register " << (int)rs << " = " << registers[rs] << "(unsigned) = " << (int16_t)registers[rs] << "(signed)" << std::endl;
			pc+=2;
			busy = false;
			stats_json["stats"]["put"] = stats_json["stats"]["put"].asInt() + 1;
			break;
		
		case LIZ:
			std::cout<<"Executing LIZ instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = imm8;
			pc += 2;
			busy = false;
			stats_json["stats"]["liz"] = stats_json["stats"]["liz"].asInt() + 1;
			break;

		case LIS:
			std::cout<<"Executing LIS instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = s_ext(imm8);
			pc += 2;
			busy = false;
			stats_json["stats"]["lis"] = stats_json["stats"]["lis"].asInt() + 1;
			break;

		case LUI:
			std::cout<<"Executing LUI instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			registers[rd] = concat8bit(imm8, (rd & 0xFF));
			pc += 2;
			busy = false;
			stats_json["stats"]["lui"] = stats_json["stats"]["lui"].asInt() + 1;
			break;

		case BP:
			std::cout<<"Executing BP instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = ((int16_t) registers[rd]) > 0 ? imm8 << 1 : pc + 2;
			busy = false;
			stats_json["stats"]["bp"] = stats_json["stats"]["bp"].asInt() + 1;
			break;

		case BN:
			std::cout<<"Executing BN instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = ((int16_t) registers[rd]) < 0 ? imm8 << 1 : pc + 2;
			busy = false;
			stats_json["stats"]["bn"] = stats_json["stats"]["bn"].asInt() + 1;
			break;

		case BX:
			std::cout<<"Executing BX instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = registers[rd] != 0 ? imm8 << 1 : pc + 2;
			busy = false;
			stats_json["stats"]["bx"] = stats_json["stats"]["bx"].asInt() + 1;
			break;

		case BZ:
			std::cout<<"Executing BZ instruction"<<std::endl;
			get_i_fields(instruction, rd, imm8);
			pc = registers[rd] == 0 ? imm8 << 1 : pc + 2;
			busy = false;
			stats_json["stats"]["bz"] = stats_json["stats"]["bz"].asInt() + 1;
			break;

		case J:
			std::cout<<"Executing J instruction"<<std::endl;
			uint16_t imm11 = instruction & 0x7FF;
			imm11 = imm11 << 0x01;
			pc += 2;
			busy = false;
			stats_json["stats"]["j"] = stats_json["stats"]["j"].asInt() + 1;
			break;
	}
	if(opcode == HALT)
	{
		terminate = true;
	}
}


}
}
