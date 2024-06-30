#include "../inc/assembler.hpp"
#include "../inc/lexer.hpp"
#include "../inc/parser.hpp"
#include "../inc/util.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 4 || string(argv[1]) != "-o")
  {
    cout << "ERROR: Bad arguments" << endl;
    return -1;
  }

  string inputFileName = "tests/" + string(argv[3]);
  FILE *inputFile = fopen(inputFileName.c_str(), "r");
  if (!inputFile)
  {
    cout << "ERROR: Input file can't be opened" << endl;
    return -1;
  }

  extern Assembler *assembler;
  assembler = new Assembler();

  assembler->init(inputFileName);
  yyin = inputFile;
  yyparse();

  string outputName = argv[2];
  ofstream outputFile(outputName);
  if (!outputFile)
  {
    cout << "ERROR | Failed to open the file: " << outputName << endl;
    return -1;
  }
  assembler->printOutput(outputFile);

  return 0;
}

void Assembler::init(string str)
{
  inputFileName = str;
  locationCounter = 0;
  currentSection = 0;

  sectionTable.push_back({"UND"});

  cout << "ASSEMBLER | " << inputFileName << ": Start" << endl;
}

void Assembler::printOutput(ofstream &os)
{
  printSymbols(os, symbolTable);
  printSections(os, sectionTable);
  printRelocations(os, sectionTable);
}

int Assembler::getSymbolId(string str)
{
  for (int i = 0; i < symbolTable.size(); i++)
  {
    if (symbolTable[i].name == str)
    {
      return i;
    }
  }

  return -1;
}

int Assembler::addToSymbolTable(string str)
{
  symbolTable.push_back(SymbolEntry({str}));

  return symbolTable.size() - 1;
}

void Assembler::addToSectionRelocs(string str, int off)
{
  int symbolId = getSymbolId(str);
  if (symbolId == -1)
  {
    cout << "ERROR | Symbol " << str << " is not in the table" << endl;
    exit(-1);
  }
  else
  {
    sectionTable[currentSection].relocs.push_back(RelocationEntry({off, symbolId}));
  }
}

void Assembler::fillMemoryIncLc(char byte1, char byte2, char byte3, char byte4)
{
  // cout << inputFileName << ": Filling memory " << hex << (int)byte1 << " " << (int)byte2 << " " << (int)byte3 << " " << (int)byte4 << endl;
  vector<char> bytes = {byte1, byte2, byte3, byte4};
  sectionTable[currentSection].memory.insert(sectionTable[currentSection].memory.end(), bytes.begin(), bytes.end());
  locationCounter += 4; // instruction size = 4B
}

void Assembler::poolSymbol(char op, char mod, char a, char b, char c, string str)
{
  for (auto &symbol : symbolTable)
  {
    if (symbol.offset > locationCounter && symbol.sectionId == currentSection)
    {
      symbol.offset += 8; // 2 instructions
    }
  }
  sectionTable[currentSection].size += 8; // 2 instructions

  fillMemoryIncLc(op | mod, (a << 4) | b, (c << 4) | getByte(complement2(4), 1), complement2(4) & 0xFF);
  _jmp(4);
  addToSectionRelocs(str, locationCounter);
  fillMemoryIncLc(0, 0, 0, 0); // placeholder
}

bool Assembler::poolNeeded(int literal)
{
  constexpr auto MAX = numeric_limits<int>::max() >> (numeric_limits<int>::digits - 11);
  constexpr auto MIN = numeric_limits<int>::min() >> (numeric_limits<int>::digits - 11);

  return literal > MAX || literal < MIN;
}

void Assembler::poolLiteral(int literal, vector<char> noPool, vector<char> pool)
{
  // Choose the correct pool based on whether a literal pool is needed
  vector<char> &chosenPool = poolNeeded(literal) ? pool : noPool;

  char op = chosenPool[0];
  char mode = chosenPool[1];
  char A = chosenPool[2];
  char B = chosenPool[3];
  char C = chosenPool[4];

  if (poolNeeded(literal))
  {
    for (auto &symbol : symbolTable)
    {
      if (symbol.offset > locationCounter && symbol.sectionId == currentSection)
      {
        symbol.offset += 8; // 2 instructions
      }
    }
    sectionTable[currentSection].size += 8; // 2 instructions

    fillMemoryIncLc(op | mode, (A << 4) | B, (C << 4) | getByte(complement2(4), 1), complement2(4) & 0xFF);
    _jmp(4);
    fillMemoryIncLc(getByte(literal, 0), getByte(literal, 1), getByte(literal, 2), getByte(literal, 3));
  }
  else
  {
    // 12b literal
    fillMemoryIncLc(op | mode, (A << 4) | B, (C << 4) | getByte((literal & 0xFFF), 1), literal & 0xFF);
  }
}

void Assembler::_global(string str)
{
  if (getSymbolId(str) == -1)
  {
    symbolTable[addToSymbolTable(str)].isGlobal = true;
  }
}

void Assembler::_extern(string str)
{
  if (getSymbolId(str) == -1)
  {
    symbolTable[addToSymbolTable(str)].isGlobal = true;
  }
}

void Assembler::_section(string str)
{
  // If there is a current section, update its size
  if (currentSection != 0)
  {
    sectionTable[currentSection].size = locationCounter;
  }

  // Create a new section
  locationCounter = 0;
  currentSection = sectionTable.size();
  sectionTable.push_back(SectionEntry({str}));
}

void Assembler::_word(string str)
{
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  else
  {
    addToSectionRelocs(str, locationCounter);
  }

  fillMemoryIncLc(0, 0, 0, 0);
}

void Assembler::_word(int literal)
{
  fillMemoryIncLc(getByte(literal, 0), getByte(literal, 1), getByte(literal, 2), getByte(literal, 3));
}

void Assembler::_skip(int literal)
{
  sectionTable[currentSection].memory.insert(sectionTable[currentSection].memory.end(), literal, 0);
  locationCounter += literal;
}

void Assembler::_end()
{
  if (currentSection != 0)
  {
    sectionTable[currentSection].size = locationCounter;
  }

  // Put sections in symbol table
  int add = 0;
  for (int i = sectionTable.size() - 1; i >= 0; i--)
  {
    SymbolEntry entry = SymbolEntry({sectionTable[i].name, i, 0, false, true});
    symbolTable.insert(symbolTable.begin(), entry);
    add++;
  }

  // Update relocs
  for (auto &section : sectionTable)
  {
    for (auto &reloc : section.relocs)
    {
      reloc.symbolId += add;
    }
  }

  locationCounter = 0;
  cout << "ASSEMBLER | " << inputFileName << ": End" << endl;
}

void Assembler::_label(string str)
{
  string name = str.substr(0, str.length() - 1); // remove ':'
  // cout << "Label: " << name << endl;
  int i = getSymbolId(name);

  if (i == -1)
  {
    i = addToSymbolTable(name);
  }
  else if (symbolTable[i].sectionId != 0)
  {
    cout << "ERROR | Label " << name << " is already defined" << endl;
    exit(-1);
  }

  symbolTable[i].offset = locationCounter;
  symbolTable[i].sectionId = currentSection;
}

void Assembler::_halt()
{
  // Zaustavlja izvršavanje instrukcija
  fillMemoryIncLc(HALT_OC, 0, 0, 0);
}

void Assembler::_int()
{
  // Izaziva softverski prekid
  fillMemoryIncLc(INT_OC, 0, 0, 0);
}

void Assembler::_iret()
{
  // pop pc; pop status;
  fillMemoryIncLc(LOAD_OC | LOAD_MOD1, (SP_REG << 4) | SP_REG, getByte(complement2(8), 1) & 0xF, complement2(8) & 0xFF);
  fillMemoryIncLc(LOAD_OC | LOAD_MOD6, (STATUS_REG << 4) | SP_REG, getByte(complement2(-4), 1) & 0xF, complement2(-4) & 0xFF);
  fillMemoryIncLc(LOAD_OC | LOAD_MOD2, (PC_REG << 4) | SP_REG, getByte(complement2(-8), 1) & 0xF, complement2(-8) & 0xFF);
}

void Assembler::_call(string str)
{
  // push pc; pc <= operand;
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(CALL_OC, CALL_MOD1, PC_REG, 0, 0, str);
}

void Assembler::_call(int literal)
{
  // -||-
  // cout << "Call literal: " << literal << endl;
  vector<char> noPool = {CALL_OC, CALL_MOD0, 0, 0, 0};
  vector<char> pool = {CALL_OC, CALL_MOD1, PC_REG, 0, 0};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_ret()
{
  // pop pc;
  _pop(PC_REG);
}

void Assembler::_jmp(string str)
{
  // pc <= operand;
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(JUMP_OC, JMP_MOD4, PC_REG, 0, 0, str);
}

void Assembler::_jmp(int literal)
{
  // -||-
  vector<char> noPool = {JUMP_OC, JMP_MOD0, PC_REG, 0, 0};
  vector<char> pool = {JUMP_OC, JMP_MOD4, PC_REG, 0, 0};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_beq(int gpr1, int gpr2, string str)
{
  // if (gpr1 == gpr2) pc <= operand;
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(JUMP_OC, JMP_MOD5, PC_REG, (char)gpr1, (char)gpr2, str);
}

void Assembler::_beq(int gpr1, int gpr2, int literal)
{
  // -||-
  vector<char> noPool = {JUMP_OC, JMP_MOD1, 0, (char)gpr1, (char)gpr2};
  vector<char> pool = {JUMP_OC, JMP_MOD5, PC_REG, (char)gpr1, (char)gpr2};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_bne(int gpr1, int gpr2, string str)
{
  // if (gpr1 != gpr2) pc <= operand;
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(JUMP_OC, JMP_MOD6, PC_REG, (char)gpr1, (char)gpr2, str);
}

void Assembler::_bne(int gpr1, int gpr2, int literal)
{
  // -||-
  vector<char> noPool = {JUMP_OC, JMP_MOD2, 0, (char)gpr1, (char)gpr2};
  vector<char> pool = {JUMP_OC, JMP_MOD6, PC_REG, (char)gpr1, (char)gpr2};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_bgt(int gpr1, int gpr2, string str)
{
  // if (gpr1 signed> gpr2) pc <= operand;
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(JUMP_OC, JMP_MOD7, PC_REG, (char)gpr1, (char)gpr2, str);
}

void Assembler::_bgt(int gpr1, int gpr2, int literal)
{
  // -||-
  vector<char> noPool = {JUMP_OC, JMP_MOD3, 0, (char)gpr1, (char)gpr2};
  vector<char> pool = {JUMP_OC, JMP_MOD7, PC_REG, (char)gpr1, (char)gpr2};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_push(int gpr)
{
  // sp <= sp - 4; mem32[sp] <= gpr;
  fillMemoryIncLc(STORE_OC | STORE_MOD2, SP_REG << 4, gpr << 4 | getByte(complement2(-4), 1), complement2(-4) & 0xFF);
}

void Assembler::_pop(int gpr)
{
  // gpr <= mem32[sp]; sp <= sp + 4;
  fillMemoryIncLc(LOAD_OC | LOAD_MOD3, (gpr << 4) | SP_REG, (0 << 4) | getByte(complement2(4), 1), complement2(4) & 0xFF);
}

void Assembler::_xchg(int gpr1, int gpr2)
{
  // temp <= gprD; gprD <= gprS; gprS <= temp;
  fillMemoryIncLc(XCHG_OC, gpr1, gpr2 << 4, 0);
}

void Assembler::_add(int gprS, int gprD)
{
  // gprD <= gprD + gprS;
  fillMemoryIncLc(ARIT_OC | ADD_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_sub(int gprS, int gprD)
{
  // gprD <= gprD - gprS;
  fillMemoryIncLc(ARIT_OC | SUB_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_mul(int gprS, int gprD)
{
  // gprD <= gprD * gprS;
  fillMemoryIncLc(ARIT_OC | MUL_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_div(int gprS, int gprD)
{
  // gprD <= gprD / gprS;
  fillMemoryIncLc(ARIT_OC | DIV_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_not(int gpr)
{
  // gpr <= ~gpr;
  fillMemoryIncLc(LOGIC_OC | NOT_MOD, (gpr << 4) | gpr, 0, 0);
}

void Assembler::_and(int gprS, int gprD)
{
  // gprD <= gprD & gprS;
  fillMemoryIncLc(LOGIC_OC | AND_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_or(int gprS, int gprD)
{
  // gprD <= gprD | gprS;
  fillMemoryIncLc(LOGIC_OC | OR_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_xor(int gprS, int gprD)
{
  // gprD <= gprD ^ gprS;
  fillMemoryIncLc(LOGIC_OC | XOR_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_shl(int gprS, int gprD)
{
  // gprD <= gprD << gprS;
  fillMemoryIncLc(SHIFT_OC | SHL_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_shr(int gprS, int gprD)
{
  // gprD <= gprD >> gprS;
  fillMemoryIncLc(SHIFT_OC | SHR_MOD, (gprD << 4) | gprD, gprS << 4, 0);
}

void Assembler::_ldImm(int literal, int gprD)
{
  // gpr[A]<=gpr[B]+D;
  vector<char> noPool = {(char)LOAD_OC, LOAD_MOD1, (char)gprD, 0, 0};
  vector<char> pool = {(char)LOAD_OC, LOAD_MOD2, (char)gprD, 0, PC_REG};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_ldImm(string str, int gprD)
{
  // -||-
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol(char(LOAD_OC), LOAD_MOD2, (char)gprD, 0, PC_REG, str);
}

void Assembler::_ldRegDir(int gprS, int gprD)
{
  // gpr[A]<=gpr[B]+D;
  fillMemoryIncLc(LOAD_OC | LOAD_MOD1, (gprD << 4) | gprS, 0, 0);
}

void Assembler::_ldRegInd(int gprS, int gprD)
{
  // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
  fillMemoryIncLc(LOAD_OC | LOAD_MOD2, (gprD << 4) | gprS, 0, 0);
}

void Assembler::_ldRegIndOff(int gprS, int literal, int gprD)
{
  // 
  if (poolNeeded(literal))
  {
    cout << "ERROR | Literal can't fit into 12b" << endl;
    exit(-1);
  }
  else
  {
    fillMemoryIncLc(LOAD_OC | LOAD_MOD2, (gprD << 4) | gprS, getByte((literal & 0xFFF), 1), literal & 0xFF);
  }
}

void Assembler::_ldMemDir(int literal, int gprD)
{
  // 
  if (poolNeeded(literal))
  {
    // pool is needed => split into 2 instructions
    vector<char> noPool = {(char)LOAD_OC, LOAD_MOD0, 0, PC_REG, 0};
    vector<char> pool = {(char)LOAD_OC, LOAD_MOD2, char(gprD), PC_REG, 0};
    poolLiteral(literal, noPool, pool);

    _ldRegInd(gprD, gprD);
  }
  else
  {
    // literal can fit into 12 bits
    fillMemoryIncLc(LOAD_OC | LOAD_MOD2, (gprD << 4), getByte((literal & 0xFFF), 1), literal & 0xFF);
  }
}

void Assembler::_ldMemDir(string str, int gprD)
{
  // -||-
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol((char)LOAD_OC, LOAD_MOD2, char(gprD), PC_REG, 0, str);
  _ldRegInd(gprD, gprD);
}

void Assembler::_stMemDir(int gprS, int literal)
{
  // mem32[D]<=gpr[C];
  vector<char> noPool = {(char)STORE_OC, STORE_MOD0, 0, 0, (char)gprS};
  vector<char> pool = {(char)STORE_OC, STORE_MOD1, PC_REG, 0, (char)gprS};
  poolLiteral(literal, noPool, pool);
}

void Assembler::_stMemDir(int gprS, string str)
{
  // -||-
  if (getSymbolId(str) == -1)
  {
    addToSymbolTable(str);
    // forwardLinkTable.push_back(ForwardLinkEntry({currentSection, locationCounter, str}));
  }
  poolSymbol((char)STORE_OC, STORE_MOD1, PC_REG, 0, (char)gprS, str);
}

void Assembler::_stRegInd(int gprS, int gprD)
{
  // 
  fillMemoryIncLc(STORE_OC | STORE_MOD0, gprD, gprS << 4, 0);
}

void Assembler::_stRegIndOff(int gprS, int gprD, int literal)
{
  // 
  if (poolNeeded(literal))
  {
    cout << "ERROR | Literal can't fit into 12b" << endl;
    exit(-1);
  }
  else
  {
    fillMemoryIncLc(STORE_OC | STORE_MOD0, gprD, (gprS << 4) | getByte((literal & 0xFFF), 1), literal & 0xFF);
  }
}

void Assembler::_csrrd(int csrS, int gprD)
{
  // gpr <= csr;
  fillMemoryIncLc(LOAD_OC | LOAD_MOD0, (gprD << 4) | csrS, 0, 0);
}

void Assembler::_csrwr(int gprS, int csrD)
{
  // csr <= gpr;
  fillMemoryIncLc(LOAD_OC | LOAD_MOD4, (csrD << 4) | gprS, 0, 0);
}