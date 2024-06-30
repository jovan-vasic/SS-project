%{
  #include "../inc/parser.hpp"
  
  #include <iostream>
  using namespace std;
%}

%option header-file="./inc/lexer.hpp"
%option outfile="./src/lexer.cpp" 
%option noyywrap

%%
[#][^\n]*                 { /* skip comments */ }
[ \t]                     { /* skip whitspace */ }
\n                        { return EOL;}

"["                       { return '['; }
"]"                       { return ']'; }
[%]                       { return '%'; }
[+]                       { return '+'; }
[*]                       { return '*'; }
[:]                       { return ':'; }
[,]                       { return ','; }
[$]                       { return '$'; }

.global                   { return GLOBAL; }
.extern                   { return EXTERN; }
.section                  { return SECTION; }
.word                     { return WORD; }
.skip                     { return SKIP; }
.end                      { return END; }

halt                      { return HALT; }
int                       { return INT; }
iret                      { return IRET; }
call                      { return CALL; }
ret                       { return RET; }
jmp                       { return JMP; }
beq                       { return BEQ; }
bne                       { return BNE; }
bgt                       { return BGT; }
push                      { return PUSH; }
pop                       { return POP; }
xchg                      { return XCHG; }
add                       { return ADD; }
sub                       { return SUB; }
mul                       { return MUL; }
div                       { return DIV; }
not                       { return NOT; }
and                       { return AND; }
or                        { return OR; }
xor                       { return XOR; }
shl                       { return SHL; }
shr                       { return SHR; }
ld                        { return LD; }
st                        { return ST; }
csrrd                     { return CSRRD; }
csrwr                     { return CSRWR; }

0[xX][a-fA-F0-9]+         { sscanf(yytext, "%x", &yylval.intVal); return NUMBER; }
[-]?[0-9]+                { yylval.intVal = atoi(yytext); return NUMBER; }

r[0-9]                    { yylval.intVal = atoi(yytext + 1); return GPR; }
r1[0-3]                   { yylval.intVal = atoi(yytext + 1); return GPR; }
sp                        { yylval.intVal = 14; return GPR; }
pc                        { yylval.intVal = 15; return GPR; }

%status                   { yylval.intVal = 0; return CSR; }
%handler                  { yylval.intVal = 1; return CSR; }
%cause                    { yylval.intVal = 2; return CSR; }

[a-zA-Z_][a-zA-Z0-9_]*[:] { yylval.strVal = strdup(yytext); return LABEL; }
[a-zA-Z_][a-zA-Z_0-9]*    { yylval.strVal = strdup(yytext); return SYMBOL; }

<<EOF>>                   { yyterminate(); }

.                         { cout << "Unknown symbol: "<< yytext << endl; exit(-1); }
%%