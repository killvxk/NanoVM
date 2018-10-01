#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>
#include <sstream>
#include "Mapper.h"

#define SRC_TYPE 0b10000000
#define SRC_SIZE 0b01100000
#define SRC_MEM  0b00010000
#define DST_MEM  0b00001000

enum Type{
	Reg = 0,
	Immediate = 0b10000000
};

bool readLines(std::string file, std::vector<std::string> &lines) {
	std::string line;
	std::ifstream f(file);
	if (f.is_open()) {
		while (std::getline(f, line)) {
			// remove comments
			int index = line.find(";");
			if (index != -1) {
				if (index == 0)
					line = "";
				else
					line = line.substr(0, index);
			}
			line = std::regex_replace(line, std::regex("^\\s+|\\s+$"), ""); // trim leading and trailing whitespaces
			if (line.empty()) // Skip empty lines and comments (prefix ";")
			{
				// push empty lines to vector. This allows to keep track of line numbers in source code while assembling
				// => assembler can show line number of invalid instructions
				lines.push_back("");
				continue;
			}
			line = std::regex_replace(line, std::regex("\\s{2,}"), " "); // replace all consecutive whitespaces with single space
			line.erase(std::remove(line.begin(), line.end(), ','), line.end()); // Remove ','
			std::transform(line.begin(), line.end(), line.begin(), ::tolower); // to lowercase
			lines.push_back(line);
		}
		return true;
	}
	return false;
}

bool assemble(std::vector<std::string> &lines, std::vector<unsigned char> &bytes) {
	// iterate lines with 'i' so we have line number available
	Mapper mapper;
	std::vector<std::vector<unsigned char>> bytecode;
	for (int i = 0; i < lines.size(); i++) {
		std::string line = lines[i];
		if (line.empty())
			continue;
		std::istringstream iss(line);
		std::vector<std::string> parts(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
		std::pair<unsigned char, unsigned int> opcode;
		// Check that the instruction is valid e.g. 'mov'
		if (!mapper.mapOpcode(parts[0], opcode)) {
			std::cout << "Error on line (" << i << "): " << line << std::endl;
			std::cout << "Unknown instruction \"" << parts[0] << std::endl;
			return false;
		}
		// Check that there are required amount of parameters for the instruction e.g. 'mov reg0,reg1' requires 2
		if (opcode.second != parts.size() - 1) {
			std::cout << "Error on line (" << i << "): " << line << std::endl;
			std::cout << "Invalid amount of parameters for instruction \"" << parts[0] << "\" expected: " << opcode.second
				<< " but received: " << (parts.size() - 1) << std::endl;
			return false;
		}
		std::vector<unsigned char> instructionBytes;
		// assemble instruction with two operands
		if (opcode.second == 2) {
			unsigned char dstReg;
			bool isDstMem = false, isSrcMem = false;
			// set flags whether the operands refer to memory address (operands are to be treated as pointers)
			if (parts[1][0] == '@') {
				parts[1] = parts[1].substr(1);
				isDstMem = true;
			}
			if (parts[2][0] == '@') {
				parts[2] = parts[2].substr(1);
				isSrcMem = true;
			}
			// parse destination register
			if (!mapper.mapRegister(parts[1], dstReg)) {
				std::cout << "Error on line (" << i << "): " << line << std::endl;
				std::cout << "Invalid register name: \"" << parts[1] << "\"" << std::endl;
				return false;
			}
			// add first instruction byte
			instructionBytes.push_back((dstReg << 5) | (opcode.first));
			unsigned char srcReg;
			// parse source register if it exists (optional parameter)
			if (mapper.mapRegister(parts[2], srcReg)) {
				// second operand was register. add final instruction byte
				instructionBytes.push_back((Type::Reg | SRC_SIZE | (isSrcMem ? SRC_MEM : 0) | (isDstMem ? DST_MEM : 0)) | srcReg);
			}
			else {
				//Second parameter is immediate value. Add place holder byte
				instructionBytes.push_back(0);
				// parse the immediate value
				int size = mapper.mapImmediate(parts[2], instructionBytes);
				if (size == -1) {
					// parameter was not integer or register
					std::cout << "Error on line (" << i << "): " << line << std::endl;
					std::cout << "Unknown parameter: \"" << parts[2] << "\"";
					return false;
				}
				else if (size == -2) {
					// immediate value couldn't fit in 64bit unsinged integer...
					std::cout << "Error on line (" << i << "): " << line << std::endl;
					std::cout << "Integer too large: " << parts[2] << std::endl;
					return false;
				}
				// we now have the size of instruction. Update to the previous byte
				instructionBytes[1] = ((Type::Immediate | size | (isSrcMem ? SRC_MEM : 0) | (isDstMem ? DST_MEM : 0)));
			}
			bytecode.push_back(instructionBytes);
 		}
		else if (opcode.second == 0) {
			// Instructions w/o operands can be pushed by the opcode (e.g. halt)
			instructionBytes.push_back((opcode.first));
			bytecode.push_back(instructionBytes);
		}
		else if (opcode.second == 1) {
			// Instruction has only one operand (e.g. jz, push, pop, ...)
			unsigned char srcReg;
			bool isSrcMem = false;
			// set flags whether the operands refer to memory address (operands are to be treated as pointers)
			if (parts[1][0] == '@') {
				parts[1] = parts[1].substr(1);
				isSrcMem = true;
			}
			// Check if the single operand is register
			if (mapper.mapRegister(parts[1], srcReg)) {
				// Operand is register
				instructionBytes.push_back((opcode.first));
				instructionBytes.push_back((Type::Reg | SRC_SIZE | (isSrcMem ? SRC_MEM : 0) | srcReg));
			}
			else {
				// The single operand is immediate value
				instructionBytes.push_back(opcode.first);
				//Second parameter is immediate value. Add place holder byte
				instructionBytes.push_back(0);
				// parse the immediate value
				int size = mapper.mapImmediate(parts[1], instructionBytes);
				if (size == -1) {
					// parameter was not integer or register
					std::cout << "Error on line (" << i << "): " << line << std::endl;
					std::cout << "Unknown parameter: \"" << parts[2] << "\"";
					return false;
				}
				else if (size == -2) {
					// immediate value couldn't fit in 64bit unsinged integer...
					std::cout << "Error on line (" << i << "): " << line << std::endl;
					std::cout << "Integer too large: " << parts[1] << std::endl;
					return false;
				}
				// we now have the size of instruction. Update to the previous byte
				instructionBytes[1] = ((Type::Immediate | size | (isSrcMem ? SRC_MEM : 0)));
			}
			bytecode.push_back(instructionBytes);
		}
	}
	for (auto instruction : bytecode)
		for (auto b : instruction)
			bytes.push_back(b);
	return true;
}

int main(int argc, char *argv[])
{
	if (argc <= 1) {
		std::cout << "Usage NanoAssembler.exe [FILE]" << std::endl;
		return 0;
	}
	std::string input = argv[1];
	std::string output = input.substr(0, input.find_last_of('.')) + ".bin";
	std::vector<std::string> lines;
	std::vector<unsigned char> bytecode;
	readLines(argv[1], lines);
	if (assemble(lines, bytecode)) {
		std::ofstream file(output, std::ios::out | std::ios::binary);
		file.write((const char*)&bytecode[0], bytecode.size());
		file.close();
		std::cout << "Bytecode assembled!" << std::endl;
	}
	system("pause");
	return 0;
}
