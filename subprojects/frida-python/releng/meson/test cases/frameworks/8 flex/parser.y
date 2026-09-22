%{
extern int yylex(void);
extern int yyerror();
%}

%token BOOLEAN

%%
input:
  BOOLEAN { $$ = $1;}
;
