#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP

#include "../inc/util.hpp"

class Assembler
{
private:
  string inputFileName;
  int locationCounter = 0;
  int currentSection = 0;

  vector<SymbolEntry> symbolTable;
  vector<SectionEntry> sectionTable;

public:
  Assembler() {}
  ~Assembler() {}

  void init(string);
  void printOutput(ofstream &);
  int getSymbolId(string);
  int addToSymbolTable(string);
  void addToSectionRelocs(string, int);
  void fillMemoryIncLc(char, char, char, char);
  void poolSymbol(char, char, char, char, char, string);
  bool poolNeeded(int);
  void poolLiteral(int, vector<char>, vector<char>);
  void _global(string);
  void _extern(string);
  void _section(string);
  void _word(string);
  void _word(int);
  void _skip(int);
  void _end();
  void _label(string);
  void _halt();
  void _int();
  void _iret();
  void _call(string);
  void _call(int);
  void _ret();
  void _jmp(string);
  void _jmp(int);
  void _beq(int, int, int);
  void _beq(int, int, string);
  void _bne(int, int, int);
  void _bne(int, int, string);
  void _bgt(int, int, int);
  void _bgt(int, int, string);
  void _push(int);
  void _pop(int);
  void _xchg(int, int);
  void _add(int, int);
  void _sub(int, int);
  void _mul(int, int);
  void _div(int, int);
  void _not(int);
  void _and(int, int);
  void _or(int, int);
  void _xor(int, int);
  void _shl(int, int);
  void _shr(int, int);
  void _ldImm(int, int);
  void _ldImm(string, int);
  void _ldRegDir(int, int);
  void _ldRegInd(int, int);
  void _ldRegIndOff(int, int, int);
  void _ldMemDir(int, int);
  void _ldMemDir(string, int);
  void _stMemDir(int, int);
  void _stMemDir(int, string);
  void _stRegInd(int, int);
  void _stRegIndOff(int, int, int);
  void _csrrd(int, int);
  void _csrwr(int, int);
};

#endif