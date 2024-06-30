%{
  #include "../inc/assembler.hpp"

  #include <iostream>

  using namespace std;
  
  extern int yylex();
  extern char* yytext;
  extern int yylineno;
  void yyerror(const char *s);

  Assembler* assembler;
%}

%defines "./inc/parser.hpp"
%output "./src/parser.cpp"

%union {
  int intVal;
  char* strVal;
}

%token <intVal> NUMBER
%token <intVal> GPR
%token <intVal> CSR
%token <strVal> SYMBOL
%token <strVal> LABEL

%token EOL
%token '+'
%token ','
%token '$'
%token '%'
%token '['
%token ']'

%token GLOBAL
%token EXTERN
%token SECTION
%token WORD
%token SKIP
%token END

%token HALT
%token INT
%token IRET
%token CALL
%token RET
%token JMP
%token BEQ
%token BNE
%token BGT
%token PUSH
%token POP
%token XCHG
%token ADD
%token SUB
%token MUL
%token DIV
%token NOT
%token AND
%token OR
%token XOR
%token SHL
%token SHR
%token LD
%token ST
%token CSRRD
%token CSRWR

%token STATUS
%token HANDLER
%token CAUSE

%%
input:
  | line input
;

line:
  EOL                  { yylineno++; }
  | directive EOL      { yylineno++; }
  | instructions EOL   { yylineno++; }
  | LABEL EOL          { yylineno++; assembler->_label($1); }
  | END EOL            { yylineno++; assembler->_end(); }

directive:
  GLOBAL globals {}
  | EXTERN externs {}
  | SECTION SYMBOL          { assembler->_section($2); }
  | WORD words
  | SKIP NUMBER             { assembler->_skip($2); }

externs:
  SYMBOL                    { assembler->_extern($1); }
  | externs ',' SYMBOL      { assembler->_extern($3); }

globals:  
  SYMBOL                    { assembler->_global($1); }
  | globals ',' SYMBOL      { assembler->_global($3); }

words: 
  SYMBOL                    { assembler->_word($1); }
  | NUMBER                  { assembler->_word($1); }
  | words ',' SYMBOL        { assembler->_word($3); }
  | words  ',' NUMBER       { assembler->_word($3); }

instructions:
  calls                     
  | jmps                            
  | beqs                    
  | bnes                    
  | bgts                    
  | regs                    
  | lds                     
  | sts                     
  | other                   

calls:
  CALL NUMBER         { assembler->_call($2); }          
  | CALL SYMBOL       { assembler->_call($2); }

jmps:
  JMP NUMBER          { assembler->_jmp($2); }
  | JMP SYMBOL        { assembler->_jmp($2); }

beqs:
  BEQ '%' GPR ',' '%' GPR ',' NUMBER        { assembler->_beq($3, $6, $8); }
  | BEQ '%' GPR ',' '%' GPR ',' SYMBOL      { assembler->_beq($3, $6, $8); }

bnes:
  BNE '%' GPR ',' '%' GPR ',' NUMBER        { assembler->_bne($3, $6, $8); }
  | BNE '%' GPR ',' '%' GPR ',' SYMBOL      { assembler->_bne($3, $6, $8); }

bgts:
  BGT '%' GPR ',' '%' GPR ',' NUMBER        { assembler->_bgt($3, $6, $8); }
  | BGT '%' GPR ',' '%' GPR ',' SYMBOL      { assembler->_bgt($3, $6, $8); }

regs:
  PUSH '%' GPR                    { assembler->_push($3); }
  | POP '%' GPR                   { assembler->_pop($3); }
  | XCHG '%' GPR ',' '%' GPR      { assembler->_xchg($3, $6); }
  | ADD '%' GPR ',' '%' GPR       { assembler->_add($3, $6); }
  | SUB '%' GPR ',' '%' GPR       { assembler->_sub($3, $6); }
  | MUL '%' GPR ',' '%' GPR       { assembler->_mul($3, $6); }
  | DIV '%' GPR ',' '%' GPR       { assembler->_div($3, $6); }
  | NOT '%' GPR                   { assembler->_not($3); }
  | AND '%' GPR ',' '%' GPR       { assembler->_and($3, $6); }
  | OR '%' GPR ',' '%' GPR        { assembler->_or($3, $6); }
  | XOR '%' GPR ',' '%' GPR       { assembler->_xor($3, $6); }
  | SHL '%' GPR ',' '%' GPR       { assembler->_shl($3, $6); }
  | SHR '%' GPR ',' '%' GPR       { assembler->_shr($3, $6); }
  | CSRRD CSR ',' '%' GPR         { assembler->_csrrd($2, $5); }
  | CSRWR '%' GPR ',' CSR         { assembler->_csrwr($3, $5); }

lds:
  LD '$' NUMBER ',' '%' GPR                       { assembler->_ldImm($3, $6); }
  | LD '$' SYMBOL ',' '%' GPR                     { assembler->_ldImm($3, $6); }
  | LD NUMBER ',' '%' GPR                         { assembler->_ldMemDir($2, $5); }
  | LD SYMBOL ',' '%' GPR                         { assembler->_ldMemDir($2, $5); }
  | LD '%' GPR ',' '%' GPR                        { assembler->_ldRegDir($3, $6); }
  | LD '[' '%' GPR ']' ',' '%' GPR                { assembler->_ldRegInd($4, $8); }
  | LD '[' '%' GPR '+' NUMBER ']' ',' '%' GPR     { assembler->_ldRegIndOff($4, $6, $10); }
  | LD '[' '%' GPR '+' SYMBOL ']' ',' '%' GPR     { cout << "REG_IND_OFF can't work with SYMBOL" << endl; exit(-1); }

sts:
  ST '%' GPR ',' '$' NUMBER                       { cout << "STORE can't work with IMMEDIATE"; exit(-1); }
  | ST '%' GPR ',' '$' SYMBOL                     { cout << "STORE can't work with IMMEDIATE"; exit(-1); }
  | ST '%' GPR ',' NUMBER                         { assembler->_stMemDir($3, $5); }
  | ST '%' GPR ',' SYMBOL                         { assembler->_stMemDir($3, $5); }
  | ST '%' GPR ',' '%' GPR                        { cout << "REG_DIR can't work with REG"; exit(-1); }
  | ST '%' GPR ',' '[' '%' GPR ']'                { assembler->_stRegInd($3, $7); }
  | ST '%' GPR ',' '[' '%' GPR '+' NUMBER ']'     { assembler->_stRegIndOff($3, $7, $9); }
  | ST '%' GPR ',' '[' '%' GPR '+' SYMBOL ']'     { cout << "REG_IND_OFF can't work with SYMBOL" << endl; exit(-1); } 

other:
  IRET                      { assembler->_iret(); }
  | HALT                    { assembler->_halt(); }
  | INT                     { assembler->_int(); }
  | RET                     { assembler->_ret(); }
%%

void yyerror(const char *str) {
  cout << "Syntax error (line " << yylineno << "): "<< "'" << yytext << "'" << endl;
}