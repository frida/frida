#include"parser.tab.h"
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>

extern int yyparse();

int main(int argc, char **argv) {
    int input;
    if(argc != 2) {
        printf("%s <input file>\n", argv[0]);
        return 1;
    }
    input = open(argv[1], O_RDONLY);
    dup2(input, STDIN_FILENO);
    close(input);
    return yyparse();
}

int yywrap(void) {
     return 0;
}

int yyerror(void) {
     printf("Parse error\n");
     exit(1);
}
