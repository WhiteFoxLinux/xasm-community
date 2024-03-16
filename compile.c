#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

const char* OS = "linux";
const char* ARCH = "amd64";
const char* VERSION = "a0.0.4_debug"; 

void print_help(char** argv){
    printf("XASM Compile %s Community Version (%s_%s)\n",OS,VERSION,ARCH);
    printf("Usage: %s [<options>] [<arguments> ...] [input file]\n",argv[0]);
    printf("\nOptions:\n");
    printf("  -h, --help\t\tshow this message, then exit\n");
    printf("  -v, --version\t\tshow compile version number, then exit\n");
    printf("  -o, --output FILE\tset output file\n");
}

typedef struct //存储从命令行参数中得到的各项数据与一些获取到的flag
{
    char* input_fpath;
    char* output_fpath;
    int exit;
}Option;

int str_eq(char* s1,char* s2){
    for(int index = 0;;index++){
        if(s1[index] != s2[index]){
            return 0;
        }
        if(s1[index] == '\0'){
            return 1;
        }
    }
}

int str_eqi(char* s1,char* s2){
    for(int index = 0;;index++){
        if(tolower(s1[index]) != tolower(s2[index])){
            return 0;
        }
        if(s1[index] == '\0'){
            return 1;
        }
    }
}

int logger(const char* __fmt,...){
    va_list args;
    va_start(args,__fmt);
    vfprintf(stdout,__fmt,args);
    fflush(stdout);
    va_end(args);
}

Option loop_parse_options(int argc,char** argv){
    Option option;
    int has_output_file = 0;
    int has_input_file = 0;
    option.input_fpath = "main.asm";
    option.output_fpath = "main";
    option.exit = 0;

    for(int index = 1;index < argc;index++){
        char* cur_arg = argv[index];
        
        if(str_eq(cur_arg,"-h") || str_eq(cur_arg,"--help")){
            print_help(argv);
            option.exit = 1;

            break;
        }else if(str_eq(cur_arg,"-v") || str_eq(cur_arg,"--version")){
            printf("%s\n",VERSION);
            option.exit = 1;

            break;
        }else if(str_eq(cur_arg,"-o") || str_eq(cur_arg,"--output")){
            if(index+1 < argc && argv[index+1][0] != '-'){
                has_output_file = 1;
                option.output_fpath = argv[index+1];
                index++;

                continue;
            }else{
                printf("Missing output file after '%s'\n",cur_arg);
                option.exit = 1;

                break;
            }
        }else if(cur_arg[0] == '-'){
            printf("Unknown option: '%s'\n",cur_arg);
            option.exit = 1;

            break;
        }else{
            has_input_file = 1;
            option.input_fpath = cur_arg;

            continue;
        }
    }
    return option;
}

Option parse_options(int argc, char** argv){
    Option option;

    if(argc == 1){
        print_help(argv);
        option.exit = 1;
    }
    
    return loop_parse_options(argc,argv);
}

typedef enum {
    NORMAL,
    STRING,
    COMMENT,
}CommandKind;

typedef struct {
    CommandKind kind;
    int length;
    char hex;
    char valid;
    char* str;
}Command;

Command parse_command(Command command){
    command.valid = 1;
    char hex;

    if(str_eqi(command.str,"add")){
        hex = 0x01;
    }else if(str_eqi(command.str,"sub")){
        hex = 0x02;
    }else if(str_eqi(command.str,"xadd")){
        hex = 0x03;
    }else if(str_eqi(command.str,"xsub")){
        hex = 0x04;
    }else if(str_eqi(command.str,"and")){
        hex = 0x11;
    }else if(str_eqi(command.str,"or")){
        hex = 0x12;
    }else if(str_eqi(command.str,"not")){
        hex = 0x13;
    }else if(str_eqi(command.str,"mov")){
        hex = 0x21;
    }else if(str_eqi(command.str,"copy")){
        hex = 0x22;
    }else if(str_eqi(command.str,"goto")){
        hex = 0x23;
    }else if(str_eqi(command.str,"geta")){
        hex = 0x24;
    }else if(str_eqi(command.str,">")){
        hex = 0x25;
    }else if(str_eqi(command.str,"<")){
        hex = 0x26;
    }else if(str_eqi(command.str,"=")){
        hex = 0x27;
    }else if(str_eqi(command.str,"lm")){
        hex = 0x28;
    }else if(str_eqi(command.str,"rm")){
        hex = 0x29;
    }else if(str_eqi(command.str,"exit")){
        hex = 0x2a;
    }else if(str_eqi(command.str,"putc")){
        hex = 0x31;
    }else if(str_eqi(command.str,"putn")){
        hex = 0x32;
    }else if(str_eqi(command.str,"puth")){
        hex = 0x33;
    }else if(str_eqi(command.str,"getc")){
        hex = 0x34;
    }else if(str_eqi(command.str,"getn")){
        hex = 0x35;
    }else if(str_eqi(command.str,"geth")){
        hex = 0x36;
    }else if(str_eqi(command.str,"&")){
        hex = 0x41;
    }else if(str_eqi(command.str,"|")){
        hex = 0x42;
    }else if(str_eqi(command.str,"^")){
        hex = 0x43;
    }else if(str_eqi(command.str,"~")){
        hex = 0x44;
    }else if(str_eqi(command.str,"<<")){
        hex = 0x45;
    }else if(str_eqi(command.str,">>")){
        hex = 0x46;
    }else if(str_eqi(command.str,"sto")){
        hex = 0xff;
    }else if(!sscanf(command.str,"%x",&hex)){
        command.valid = 0;
        return command;
    }
    command.hex = hex;
    return command;
}

typedef enum {
    STRING_SQM,
    STRING_DQM,
}StringKind;

char* readbuf;
Command read_command(FILE* file_ptr){
    Command command;
    CommandKind kind = NORMAL;
    int readindex = 0;
    int continue_flag = 1;
    StringKind string_kind = STRING_SQM;
    for(;continue_flag;readindex++)
    {
        readbuf = (char*)realloc(readbuf,readindex+1);
        readbuf[readindex] = fgetc(file_ptr);
        
        if(readbuf[readindex] == EOF){
            break;
        }

        switch (kind)
        {
        case STRING:
            switch (string_kind)
            {
            case STRING_SQM:
                switch (readbuf[readindex])
                {
                case '\'':
                    continue_flag = 0;
                    break;
                
                case '\\':
                    readindex++;
                    readbuf[readindex] = fgetc(file_ptr);
                    switch (readbuf[readindex])
                    {
                    case EOF:
                        continue_flag = 0;
                        break;


                    case 'n':
                        readindex--;
                        readbuf[readindex] = '\n';
                        break;

                    case 't':
                        readindex--;
                        readbuf[readindex] = '\t';
                        break;

                    case 'r':
                        readindex--;
                        readbuf[readindex] = '\r';
                        break;

                    case 'x':
                        readindex--;
                        fscanf(file_ptr, "%02hhx", &readbuf[readindex]);
                        break;

                    default:
                        readindex--;
                        readbuf[readindex] = readbuf[readindex+1];
                        break;
                    }
                    break;
                default:
                    continue;
                    break;
                }
                break;

            case STRING_DQM:
                switch (readbuf[readindex])
                {
                case '"':
                    continue_flag = 0;
                    break;
                
                case '\\':
                    readindex++;
                    readbuf[readindex] = fgetc(file_ptr);
                    switch (readbuf[readindex])
                    {
                    case EOF:
                        continue_flag = 0;
                        break;

                    case 'n':
                        readindex--;
                        readbuf[readindex] = '\n';
                        break;

                    case 't':
                        readindex--;
                        readbuf[readindex] = '\t';
                        break;

                    case 'r':
                        readindex--;
                        readbuf[readindex] = '\r';
                        break;

                    case 'x':
                        readindex--;
                        fscanf(file_ptr, "%02hhx", &readbuf[readindex]);
                        break;

                    default:
                        readindex--;
                        readbuf[readindex] = readbuf[readindex+1];
                        break;
                    }
                    break;
                default:
                    continue;
                    break;
                }
                break;
            }
            break;
        
        case COMMENT:
            switch (readbuf[readindex])
            {
            case '\n':
                continue_flag = 0;
                break;
            
            default:
                continue;
                break;
            }
            break;
        case NORMAL:
            switch (readbuf[readindex])
            {
            case ' ':
                continue_flag = 0;
                break;
            
            case '\t':
                continue_flag = 0;
                break;

            case '\n':
                continue_flag = 0;
                break;

            case '\'':
                string_kind = STRING_SQM;
                kind = STRING;
                readindex--;
                break;

            case '"':
                string_kind = STRING_DQM;
                kind = STRING;
                readindex--;
                break;

            case '/':
                readindex++;
                readbuf[readindex] = fgetc(file_ptr);
                switch (readbuf[readindex])
                {
                case EOF:
                    continue_flag = 0;
                    break;
                
                case '/':
                    kind = COMMENT;
                    break;

                default:
                    break;
                }
                break;

            case '-':
                readindex++;
                readbuf[readindex] = fgetc(file_ptr);
                switch (readbuf[readindex])
                {
                case EOF:
                    continue_flag = 0;
                    break;
                
                case '-':
                    kind = COMMENT;
                    break;

                default:
                    break;
                }
                break;

            case '#':
                kind = COMMENT;
                break;

            default:
                continue;
                break;
            }
            break;
        default:
            break;
        }
        if(!continue_flag){
            break;
        }
    }
    if(readbuf[readindex] == EOF && readindex == 0){
        readbuf[readindex] = '\0';
        command.length = EOF;
        command.kind = kind;
        command.str = readbuf;
        return command;
    }
    command.length = readindex;
    command.kind = kind;
    readbuf[readindex] = '\0';
    command.str = readbuf;
    return command;
}

void compile_file(char* input_fpath,char* output_fpath){
    FILE* input_fptr = fopen(input_fpath,"r");
    FILE* output_fptr = fopen(output_fpath,"w");

    if(input_fptr == NULL){
        printf("Could not open source file '%s'\n",input_fpath);
        return;
    }
    if(output_fptr == NULL)
    {
        printf("Could not create output file '%s'\n",output_fpath);
        return;
    }

    Command command;
    while((command = read_command(input_fptr)).length != EOF){
        if(command.length == 0){
            continue;
        }

        logger("\nreader position: %zu\n",ftell(input_fptr));

        if(command.kind == COMMENT){
            logger("comment: %s\n",command.str);
            continue;
        }

        if(command.kind == STRING){
            logger("writebuffer: %s\n",command.str);
            fputs(command.str,output_fptr);
            fflush(output_fptr);
            continue;
        }

        logger("readbuffer: %s\n",command.str);

        if((command = parse_command(command)).valid){
            logger("writebuffer: %x\n",command.hex);
            fputc(command.hex,output_fptr);
            fflush(output_fptr);
        }else{
            logger("invalid command: %s\n",command.str);
        }
    }

    fclose(input_fptr);
    fclose(output_fptr);
}

int main(int argc, char **argv){
    Option options = parse_options(argc,argv);
    if(options.exit == 1){
        return 0;
    }
    char* input_fpath = options.input_fpath;
    char* output_fpath = options.output_fpath;
    logger("input filepath: %s\n",input_fpath);
    logger("output filepath: %s\n",output_fpath);
    compile_file(input_fpath,output_fpath);
    return 0;
}
