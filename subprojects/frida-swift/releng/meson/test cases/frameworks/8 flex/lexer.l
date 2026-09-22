%{
#include <stdlib.h>
#include "parser.tab.h"

extern int yylex(void);
extern int yyerror();
%}

%option noyywrap nounput noinput

%%
("true"|"false")    {return BOOLEAN;}
. { yyerror(); }
