#include <stdio.h>
#include <sys/stat.h>
//#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

const char* OS = "linux";
const char* ARCH = "amd64";
const char* VERSION = "a0.0.2_debug"; 

void print_help(char** argv){
    printf("XASM Compile %s Community Version (%s_%s)\n",OS,VERSION,ARCH);
    printf("Usage: %s [<options>] [<arguments> ...] [input file]\n",argv[0]);
    printf("\nOptions:\n");
    printf("  -h, --help\t\tshow this message, then exit\n");
    printf("  -v, --version\t\tshow compile version number, then exit\n");
    printf("  -o, --output FILE\tset output file\n");
}

// //字符串哈希
// unsigned int hash(const char* str){
//     unsigned int hash = 5381;
//     for (int index=0;index < strlen(str);index++)
//         hash = ((hash << 5) + hash) + str[index]; /* hash * 33 + c */
//     return hash;
// }

struct OptData //存储从命令行参数中得到的各项数据与一些获取到的flag
{
    char* input_filepath;
    char* output_filepath;
    int exit;
};

int str_eq(char* s1,char* s2){
    // return strcmp(s1,s2) == 0;

    // int s1_len = strlen(s1);
    // int s2_len = strlen(s2);
    // if(s1_len!=s2_len){
    //     return 0;
    // }
    // for(int index=0;index < s1_len;index++)
    //     if(s1[index] != s2[index])
    //         return 0;

    // return 1;
    
    for(int index=0;;index++){
        if(s1[index] != s2[index]){
            return 0;
        }
        if(s1[index] == '\0'){
            return 1;
        }
    }
}

int str_eqi(char* s1,char* s2){
    for(int index=0;;index++){
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

struct OptData loop_parse_options(int argc,char** argv){
    struct OptData option_data;
    int has_output_file = 0;
    int has_input_file = 0;
    option_data.input_filepath = "main.asm";
    option_data.output_filepath = "main";
    option_data.exit = 0;
    for(int index=1;index < argc;index++){
        char* cur_arg = argv[index];
        
        if(str_eq(cur_arg,"-h") || str_eq(cur_arg,"--help")){
            print_help(argv);
            option_data.exit = 1;

            break;
        }else if(str_eq(cur_arg,"-v") || str_eq(cur_arg,"--version")){
            printf("%s\n",VERSION);
            option_data.exit = 1;

            break;
        }else if(str_eq(cur_arg,"-o") || str_eq(cur_arg,"--output")){
            if(index+1 < argc && argv[index+1][0] != '-'){
                has_output_file = 1;
                option_data.output_filepath = argv[index+1];
                index++;

                continue;
            }else{
                printf("Missing output file after '%s'\n",cur_arg);
                option_data.exit = 1;

                break;
            }
        }else if(cur_arg[0] == '-'){
            printf("Unknown option: '%s'\n",cur_arg);
            option_data.exit = 1;

            break;
        }else{
            has_input_file = 1;
            option_data.input_filepath = cur_arg;

            continue;
        }
    }
    return option_data;
}

struct OptData parse_options(int argc, char** argv){
    struct OptData option_data;

    if(argc == 1){ //没有参数
        print_help(argv);
        option_data.exit = 1;
    }else{ //有参数
        option_data = loop_parse_options(argc,argv);
    }
    
    return option_data;
}

size_t get_filesize(char* filename){
    struct stat stat_buf;
    stat(filename,&stat_buf);
    size_t filesize = stat_buf.st_size;
    return filesize;
}

struct CommandHex
{
    char hex;
    char valid;
    //char* comment;
};

struct CommandHex parse_command_to_hex(char* command){
    // printf("command:%s\n",command);
    struct CommandHex command_hex;
    command_hex.valid = 1;
    //command_hex.comment = NULL;
    char hex;
    char* endptr;
    char tmp_hex = strtol(command,&endptr,16);
    if(str_eqi(command,"add")){
        hex = 0x01;
    }else if(str_eqi(command,"sub")){
        hex = 0x02;
    }else if(str_eqi(command,"xadd")){
        hex = 0x03;
    }else if(str_eqi(command,"xsub")){
        hex = 0x04;

    }else if(str_eqi(command,"and")){
        hex = 0x11;
    }else if(str_eqi(command,"or")){
        hex = 0x12;
    }else if(str_eqi(command,"not")){
        hex = 0x13;

    }else if(str_eqi(command,"mov")){
        hex = 0x21;
    }else if(str_eqi(command,"copy")){
        hex = 0x22;
    }else if(str_eqi(command,"goto")){
        hex = 0x23;
    }else if(str_eqi(command,"geta")){
        hex = 0x24;
    }else if(str_eqi(command,">")){
        hex = 0x25;
    }else if(str_eqi(command,"<")){
        hex = 0x26;
    }else if(str_eqi(command,"=")){
        hex = 0x27;
    }else if(str_eqi(command,"lm")){
        hex = 0x28;
    }else if(str_eqi(command,"rm")){
        hex = 0x29;
    }else if(str_eqi(command,"exit")){
        hex = 0x2a;
        
    }else if(str_eqi(command,"putc")){
        hex = 0x31;
    }else if(str_eqi(command,"putn")){
        hex = 0x32;
    }else if(str_eqi(command,"puth")){
        hex = 0x33;
    }else if(str_eqi(command,"getc")){
        hex = 0x34;
    }else if(str_eqi(command,"getn")){
        hex = 0x35;
    }else if(str_eqi(command,"geth")){
        hex = 0x36;

    }else if(str_eqi(command,"&")){
        hex = 0x41;
    }else if(str_eqi(command,"|")){
        hex = 0x42;
    }else if(str_eqi(command,"^")){
        hex = 0x43;
    }else if(str_eqi(command,"~")){
        hex = 0x44;
    }else if(str_eqi(command,"<<")){
        hex = 0x45;
    }else if(str_eqi(command,">>")){
        hex = 0x46;
    }else if(*endptr == '\0'){
        hex = tmp_hex;
    }else{
        command_hex.valid = 0;
        return command_hex;
    }
    command_hex.hex = hex;
    return command_hex;
}

enum CommandKind {
    NORMAL,
    STRING,
    COMMENT,
};

enum StringKind {
    STRING_SQM,
    STRING_DQM,
};

struct Command {
    enum CommandKind kind;
    int length;
};

struct Command read_command(FILE* file_ptr,char* readbuf){
    struct Command command;
    enum CommandKind kind = NORMAL;
    int readcount = 0;
    int continue_flag=1;
    enum StringKind string_kind = STRING_SQM;
    for(;continue_flag;readcount++)
    {

        readbuf[readcount] = fgetc(file_ptr);
        
        if(readbuf[readcount] == EOF){
            break;
        }

        switch (kind)
        {
        case STRING:
            switch (string_kind)
            {
            case STRING_SQM:
                switch (readbuf[readcount])
                {
                case '\'':
                    continue_flag = 0;
                    break;
                
                case '\\':
                    readcount++;
                    readbuf[readcount] = fgetc(file_ptr);
                    switch (readbuf[readcount])
                    {
                    case EOF:
                        continue_flag = 0;
                        break;


                    case 'n':
                        readcount--;
                        readbuf[readcount] = '\n';
                        break;

                    case 't':
                        readcount--;
                        readbuf[readcount] = '\t';
                        break;

                    case 'r':
                        readcount--;
                        readbuf[readcount] = '\r';
                        break;

                    case 'x':
                        readcount--;
                        logger("\\x\n");
                        fscanf(file_ptr, "%02hhx", &readbuf[readcount]);
                        //readbuf[readcount] = '\x48';
                        break;

                    default:
                        readcount--;
                        readbuf[readcount] = readbuf[readcount+1];
                        break;
                    }
                    break;
                    // if(readbuf[readcount] == EOF){
                    //     break;
                    // }
                default:
                    continue;
                    break;
                }
                break;

            case STRING_DQM:
                switch (readbuf[readcount])
                {
                case '"':
                    continue_flag = 0;
                    break;
                
                case '\\':
                    readcount++;
                    readbuf[readcount] = fgetc(file_ptr);
                    switch (readbuf[readcount])
                    {
                    case EOF:
                        continue_flag = 0;
                        break;


                    case 'n':
                        readcount--;
                        readbuf[readcount] = '\n';
                        break;

                    case 't':
                        readcount--;
                        readbuf[readcount] = '\t';
                        break;

                    case 'r':
                        readcount--;
                        readbuf[readcount] = '\r';
                        break;

                    case 'x':
                        readcount--;
                        logger("\\x\n");
                        fscanf(file_ptr, "%02hhx", &readbuf[readcount]);
                        //readbuf[readcount] = '\x48';
                        break;

                    default:
                        readcount--;
                        readbuf[readcount] = readbuf[readcount+1];
                        break;
                    }
                    break;
                    // if(readbuf[readcount] == EOF){
                    //     break;
                    // }
                default:
                    continue;
                    break;
                }
                break;
            }
            break;
        
        case COMMENT:
            switch (readbuf[readcount])
            {
            case '\n':
                continue_flag=0;
                break;
            
            default:
                continue;
                break;
            }
            break;
        case NORMAL:
            switch (readbuf[readcount])
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
                readcount--;
                break;

            case '"':
                string_kind = STRING_DQM;
                kind = STRING;
                readcount--;
                break;

            case '/':
                readcount++;
                readbuf[readcount] = fgetc(file_ptr);
                switch (readbuf[readcount])
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

            default:
                continue;
                break;
            }
            break;
        default:
            break;
        }
        if(!continue_flag){
            //readcount--;
            break;
        }
    }
    if(readbuf[readcount] == EOF && readcount == 0){
        readbuf[readcount] = '\0';
        command.length = EOF;
        command.kind = kind;
        return command;
    }
    command.length = readcount;
    command.kind = kind;
    readbuf[readcount] = '\0';
    return command;
}

void compile_file(char* input_filepath,char* output_filepath){
    FILE* input_fptr = fopen(input_filepath,"r");
    FILE* output_filepointer = fopen(output_filepath,"w");

    size_t input_filesize = get_filesize(input_filepath);
    char* readbuffer = (char*)malloc(input_filesize);

    if(input_fptr == NULL){
        printf("Could not open source file '%s'\n",input_filepath);
        return;
    }
    if(output_filepointer == NULL)
    {
        printf("Could not create output file '%s'\n",output_filepath);
        return;
    }
    enum CommandKind kind = NORMAL;
    struct Command command;
    int comp_size=0;
    int code_uc=0;
    while((command = read_command(input_fptr,readbuffer)).length != EOF){
        // printf("kind: %d\n",command.kind);

        if(readbuffer[0] == '\0'){
            continue;
        }
        logger("\nreader position: %zu\n",ftell(input_fptr));
        logger("code unit count:%d\n",++code_uc);

        if(command.kind == COMMENT){
            logger("comment: %s\n",readbuffer);
            continue;
        }

        if(command.kind == STRING){
            logger("writebuffer: %s\n",readbuffer);
            fputs(readbuffer,output_filepointer);
            fflush(output_filepointer);
            logger("compiled file size:%d\n",comp_size+=command.length);
            continue;
        }

        logger("readbuffer: %s\n",readbuffer);

        struct CommandHex command_hex;
        if((command_hex = parse_command_to_hex(readbuffer)).valid){
            logger("writebuffer: %x\n",command_hex.hex);
            fputc(command_hex.hex,output_filepointer);
            fflush(output_filepointer);
            logger("compiled file size:%d\n",++comp_size);
        }else{
            logger("invalid command: %s\n",readbuffer);
        }
    }
    logger("\nsource filesize: %zu\n",input_filesize);

    fclose(input_fptr);
    fclose(output_filepointer);
    
}

int main(int argc, char **argv){
    struct OptData options_data = parse_options(argc,argv);
    if(options_data.exit == 1){
        return 0;
    }
    char* input_filepath = options_data.input_filepath;
    char* output_filepath = options_data.output_filepath;
    logger("input filepath: %s\n",input_filepath);
    logger("output filepath: %s\n",output_filepath);
    compile_file(input_filepath,output_filepath);
    return 0;
}
