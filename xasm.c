#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdarg.h>

#if defined __linux__ || defined __APPLE__
#include <unistd.h>
#include <termios.h>
int getch(void){
    struct termios oldT, newT;
    int ch;
    tcgetattr(STDIN_FILENO, &oldT);
    newT = oldT;
    newT.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newT);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldT);
    return ch;
}
#elif defined _WIN32
#include <windows.h>
#include <conio.h>
void usleep(int usec) 
{ 
    HANDLE timer; 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}
#endif

const char* OS = "linux";
const char* ARCH = "amd64";
const char* VERSION = "a0.0.1_debug"; 

void print_help(char** argv){
    printf("XASM VM %s Community Version (%s_%s)\n",VERSION,OS,ARCH);
    printf("Usage: %s [<options>] [<arguments> ...]\n",argv[0]);
    printf("\nOptions:\n");
    printf("  -h, --help\t\tshow this message, then exit\n");
    printf("  -v, --version\t\tshow compile version number, then exit\n");
    printf("  -i, --input FILE\tset input file\n");
    printf("  -l, --log-file FILE\tset log file\n");
    printf("  -c, --cpu-clock Nhz\tlimit cpu clock\n");
}

struct OptData
{
    char* input_file;
    char* log_file;
    int cpu_clock;
    int non_log_file;
    int exit;
};

int str_eq(char* s1,char* s2){
    for(int index=0;;index++){
        if(s1[index] != s2[index]){
            return 0;
        }
        if(s1[index] == '\0'){
            return 1;
        }
    }
}

struct OptData loop_parse_options(int argc, char** argv){
    struct OptData option_data;
    option_data.exit = 0;
    option_data.input_file = "main";
    option_data.log_file = "xasm.log";
    option_data.cpu_clock = -1;
    option_data.non_log_file = 0;

    for(int index=1;index < argc;index++){
        char* cur_arg = argv[index];
        if(str_eq(cur_arg,"-h") || str_eq(cur_arg,"--help")){
            print_help(argv);
            option_data.exit = 1;
            break;
        }

        if(str_eq(cur_arg,"-v") || str_eq(cur_arg,"--version")){
            printf("%s\n",VERSION);
            option_data.exit = 1;
            break;
        }

        if(str_eq(cur_arg,"-i") || str_eq(cur_arg,"--input")){
            if(index+1 < argc && argv[index+1][0] != '-'){
                option_data.input_file = argv[index+1];
                index++;
                continue;
            }else{
                printf("Missing input file after '%s'\n",cur_arg);
                option_data.exit = 1;
                break;
            }
        }

        if(str_eq(cur_arg,"-l") || str_eq(cur_arg,"--log-file")){
            if(index+1 < argc && argv[index+1][0] != '-'){
                option_data.log_file = argv[index+1];
                index++;
                continue;
            }else if(index+1 < argc && argv[index+1][0] == '-' && argv[index+1][1] == '\0'){
                option_data.non_log_file = 1;
                index++;
                continue;
            }else{
                printf("Missing input file after '%s'\n",cur_arg);
                option_data.exit = 1;
                break;
            }
        }

        if(str_eq(cur_arg,"-c") || str_eq(cur_arg,"--cpu-clock")){
            if(index+1 < argc && argv[index+1][0] != '-'){
                if(!sscanf(argv[index+1],"%d",&option_data.cpu_clock)){
                    printf("Invalid cpu clock value: %s\n",argv[index+1]);
                    option_data.exit = 1;
                    break;
                };
                index++;
                continue;
            }else{
                printf("Missing cpu clock after '%s'\n",cur_arg);
                option_data.exit = 1;
                break;
            }
        }

        if(cur_arg[0] == '-'){
            printf("Unknown option: %s\n",cur_arg);
            print_help(argv);
            option_data.exit = 1;
            break;
        }
    }

    return option_data;
}

struct OptData parse_options(int argc, char** argv){
    struct OptData option_data;
    option_data.exit = 1;
    if(argc == 1){
        print_help(argv);
        option_data.exit = 1;
    }else{
        option_data = loop_parse_options(argc,argv);
    }
    return option_data;
};

size_t get_filesize(char* filename){
    struct stat stat_buf;
    stat(filename,&stat_buf);
    size_t filesize = stat_buf.st_size;
    return filesize;
}

FILE* log_file = NULL;
char* log_file_name = NULL;

int logger(const char *__format, ...){
    va_list args_ptr;
    if(log_file == NULL){
        return 0;
    }
    va_start(args_ptr,__format);
    int res = vfprintf(log_file,__format,args_ptr);
    fflush(log_file);
    va_end(args_ptr);
    return res;
}

int* virt_mem=NULL;
unsigned int exec_ptr=0;

enum ExecuteMode {
    EXECUTE_NORMAL=0,
    EXECUTE_STORAGE,
};

int execute_mode = EXECUTE_NORMAL;


enum {
    VOID = 0x00,
    ADD,
    SUB,
    XADD,
    XSUB,

    LAND = 0x11,
    LOR,
    LNOT,

    MOV = 0x21,
    COPY,
    GOTO,
    GETA,
    MORE,
    LESS,
    EQUA,
    LM,
    RM,
    EXIT,

    PUTC = 0x31,
    PUTN,
    PUTH,
    GETC,
    GETN,
    GETH,

    AND = 0x41,
    OR,
    XOR,
    NOT,
    SHL,
    SHR,

    STORAGE = 0xff,
};

int init_virt_memory(){
    virt_mem = realloc(virt_mem, 0xfffffffful * 4ul); //先尝试直接分配 0xffffffff 个 int 内存
    if(virt_mem == NULL){ //直接分配 失败
        virt_mem = realloc(virt_mem, 0xfffffffful * 1ul); //先分配 0xffffffff 个 1字节 内存
        virt_mem = realloc(virt_mem, 0xfffffffful * 2ul); //先分配 0xffffffff 个 2字节 内存
        virt_mem = realloc(virt_mem, 0xfffffffful * 3ul); //先分配 0xffffffff 个 3字节 内存
        virt_mem = realloc(virt_mem, 0xfffffffful * 4ul); //再分配 0xffffffff 个 4字节 内存
        if(virt_mem == NULL){
            return 1;
        }
    }

    return 0;
}

void read_coms_to_mem(char* comfile_name){
    FILE* comfile_pointer = fopen(comfile_name,"r");

    if(comfile_pointer == NULL){
        printf("Could not open file '%s'\n",comfile_name);
        exit(1);
        return;
    }

    size_t comfile_size = get_filesize(comfile_name);

    logger("program filesize: %zu\n",comfile_size);
    
    // fread(virt_memory,comfile_size,1,comfile_pointer);
    // for(size_t index=0;index<comfile_size;index++){
    //     virt_mem[index]=fgetc(comfile_pointer);
    // }
    size_t index = 0;
    while ((virt_mem[index] = fgetc(comfile_pointer)) != EOF)
    {
        index++;
    }
    

    fclose(comfile_pointer);
}

int hexsmerge(int hex1,int hex2,int hex3,int hex4){
    return (hex1 << 24) + (hex2 << 16) + (hex3 << 8) + hex4;
}

#define curhexs_merge() hexsmerge(virt_mem[exec_ptr+1],virt_mem[exec_ptr+2],virt_mem[exec_ptr+3],virt_mem[exec_ptr+4])

// 运算类

void add(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Add(0x%08x,0x%08x)\n",n1_addr,n2_addr);

    int sum = virt_mem[n1_addr] + virt_mem[n2_addr];
    virt_mem[n1_addr] = sum;
    logger("Res=0x%08x\n",sum);
}

void sub(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Sub(0x%08x,0x%08x)\n",n1_addr,n2_addr);

    int sum = virt_mem[n1_addr] - virt_mem[n2_addr];
    virt_mem[n1_addr] = sum;
    logger("Res=0x%08x\n",sum);
}

void xadd(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Xadd(0x%08x,0x%08x)\n",n1_addr,n2_addr);

    int sum = virt_mem[n1_addr] * virt_mem[n2_addr];
    virt_mem[n1_addr] = sum;
    logger("Res=0x%08x\n",sum);
}

void xsub(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Xsub(0x%08x,0x%08x)\n",n1_addr,n2_addr);

    int sum = virt_mem[n1_addr] / virt_mem[n2_addr];
    virt_mem[n1_addr] = sum;
    logger("Res=0x%08x\n",sum);
}

// 逻辑判断类

void land(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int b_addr = curhexs_merge();
    exec_ptr += 4;
    logger("LAnd(0x%08x,0x%08x,0x%08x)\n",n1_addr,n2_addr,b_addr);

    int b = virt_mem[n1_addr] && virt_mem[n2_addr];
    virt_mem[b_addr] = b;
    logger("Res=0x%08x\n",b);
}

void lor(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int b_addr = curhexs_merge();
    exec_ptr += 4;
    logger("LOr(0x%08x,0x%08x,0x%08x)\n",n1_addr,n2_addr,b_addr);

    int b = virt_mem[n1_addr] || virt_mem[n2_addr];
    virt_mem[b_addr] = b;
    logger("Res=0x%08x\n",b);
}

void lnot(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int b_addr = curhexs_merge();
    exec_ptr += 4;
    logger("LNot(0x%08x,0x%08x)\n",n1_addr,b_addr);

    int b = !virt_mem[n1_addr];
    virt_mem[b_addr] = b;
    logger("Res=0x%08x\n",b);
}

// 控制类

void mov(){
    unsigned int var_addr = curhexs_merge();
    exec_ptr += 4;
    int val = curhexs_merge();
    exec_ptr += 4;
    logger("Mov(0x%08x,0x%08x)\n",var_addr,val);

    virt_mem[var_addr] = val;
    logger("0x%08x => 0x%08x\n",var_addr,val);
}

void copy(){
    unsigned int var1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int var2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Copy(0x%08x,0x%08x)\n",var1_addr,var2_addr);

    virt_mem[var1_addr] = virt_mem[var2_addr];
    logger("Copied 0x%08x => 0x%08x\n",var1_addr,var2_addr);
}

void _goto(){
    unsigned int addr_ptr = curhexs_merge();
    logger("Goto(0x%08x)\n",addr_ptr);

    exec_ptr = virt_mem[addr_ptr] - 1;
    logger("Jump to 0x%08x\n",virt_mem[addr_ptr]);
}

void geta(){
    unsigned int addr_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("Geta(0x%08x)\n",addr_ptr);

    virt_mem[addr_ptr] = exec_ptr + 1;
    logger("Saved 0x%08x => 0x%08x\n",exec_ptr + 1,addr_ptr);
}

void more(){
    unsigned int val1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int val2_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int addr_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x > 0x%08x -> 0x%08x\n",val1_addr,val2_addr,addr_ptr);

    if(virt_mem[val1_addr] > virt_mem[val2_addr]){
        logger("Jump to 0x%08x\n",virt_mem[addr_ptr]);
        exec_ptr = virt_mem[addr_ptr] - 1;
    }else{
        logger("Continue\n");
    }
}

void less(){
    unsigned int val1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int val2_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int addr_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x < 0x%08x -> 0x%08x\n",val1_addr,val2_addr,addr_ptr);

    if(virt_mem[val1_addr] < virt_mem[val2_addr]){
        logger("Jump to 0x%08x\n",virt_mem[addr_ptr]);
        exec_ptr = virt_mem[addr_ptr] - 1;
    }else{
        logger("Continue\n");
    }
}

void equa(){
    unsigned int val1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int val2_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int addr_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x = 0x%08x -> 0x%08x\n",val1_addr,val2_addr,addr_ptr);

    if(virt_mem[val1_addr] == virt_mem[val2_addr]){
        logger("Jump to 0x%08x\n",virt_mem[addr_ptr]);
        exec_ptr = virt_mem[addr_ptr] - 1;
    }else{
        logger("Continue\n");
    }
}

void lm(){
    unsigned int val1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int count_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int val1 = virt_mem[val1_addr];

    logger("Lm(0x%08x,0x%08x)\n",val1_addr,virt_mem[count_addr]);
    // virt_mem[val1_addr] <<= virt_mem[count_addr];
    unsigned int target_addr = val1_addr - virt_mem[count_addr];
    logger("Target Addr: 0x%08x\n",target_addr);
    virt_mem[val1_addr] = 0;
    virt_mem[target_addr] = val1;
}

void rm(){
    unsigned int val1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int count_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int val1 = virt_mem[val1_addr];

    logger("Rm(0x%08x,0x%08x)\n",val1_addr,virt_mem[count_addr]);
    // virt_mem[val1_addr] <<= virt_mem[count_addr];
    unsigned int target_addr = val1_addr + virt_mem[count_addr];
    logger("Target Addr: 0x%08x\n",target_addr);
    virt_mem[val1_addr] = 0;
    virt_mem[target_addr] = val1;
}

// IO类

void _putc(){
    unsigned int char_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("Putc(0x%08d)\n",char_ptr);
    unsigned int char_addr = virt_mem[char_ptr];
    putchar(virt_mem[char_addr]);
    fflush(stdout);
    logger("Printed 0x%02x\n",virt_mem[char_addr]);
}

void putn(){
    unsigned int n_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("Putn(0x%08d)\n",n_ptr);
    unsigned int n_addr = virt_mem[n_ptr];
    printf("%d",virt_mem[n_addr]);
    fflush(stdout);
    logger("Printed 0x%02x\n",virt_mem[n_addr]);
}

void puth(){
    unsigned int hex_ptr = curhexs_merge();
    exec_ptr += 4;
    logger("Putn(0x%08d)\n",hex_ptr);
    unsigned int hex_addr = virt_mem[hex_ptr];
    printf("%x",virt_mem[hex_addr]);
    fflush(stdout);
    logger("Printed 0x%02x\n",virt_mem[hex_addr]);
}

void _getc(){
    unsigned int char_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Getc(0x%08x)\n",char_addr);

    virt_mem[char_addr] = getch();
    fflush(stdin);
    logger("Read 0x%02x\n",virt_mem[char_addr]);
}

void getn(){
    unsigned int num_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Getn(0x%08x)\n",num_addr);
    int num=0;

    if(scanf("%d",&num) != 1){
        logger("Error: Invalid Number\n");
        exec_ptr -= 4;
        getchar();
        return getn();
    }

    virt_mem[num_addr] = num;
    logger("Read 0x%02x\n",virt_mem[num_addr]);
}

void geth(){
    unsigned int hex_addr = curhexs_merge();
    exec_ptr += 4;
    logger("Geth(0x%08x)\n",hex_addr);
    int hex=0x00;

    if(scanf("%x",&hex) != 1){
        logger("Error: Invalid hex\n");
        exec_ptr -= 4;
        getchar();
        return geth();
    }

    virt_mem[hex_addr] = hex;
    logger("Read 0x%02x\n",virt_mem[hex_addr]);
}

// 位运算类

void and(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x & 0x%08x)\n",n1_addr,n2_addr);
    virt_mem[n1_addr]&=virt_mem[n2_addr];
    logger("Res=0x%08x\n",virt_mem[n1_addr]);
}

void or(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x | 0x%08x)\n",n1_addr,n2_addr);
    virt_mem[n1_addr]|=virt_mem[n2_addr];
    logger("Res=0x%08x\n",virt_mem[n1_addr]);
}

void xor(){
    unsigned int n1_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int n2_addr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x ^ 0x%08x)\n",n1_addr,n2_addr);
    virt_mem[n1_addr]^=virt_mem[n2_addr];
    logger("Res=0x%08x\n",virt_mem[n1_addr]);
}

void not(){
    unsigned int n_addr = curhexs_merge();
    exec_ptr += 4;
    logger("~ 0x%08x\n",n_addr);
    virt_mem[n_addr]=~virt_mem[n_addr];
    logger("Res=0x%08x\n",virt_mem[n_addr]);
}

void shl(){
    unsigned int n_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int count_addr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x << 0x%08x\n",n_addr,count_addr);
    virt_mem[n_addr]<<=virt_mem[count_addr];
    logger("Res=0x%08x\n",virt_mem[n_addr]);
}

void shr(){
    unsigned int n_addr = curhexs_merge();
    exec_ptr += 4;
    unsigned int count_addr = curhexs_merge();
    exec_ptr += 4;
    logger("0x%08x >> 0x%08x\n",n_addr,count_addr);
    virt_mem[n_addr]>>=virt_mem[count_addr];
    logger("Res=0x%08x\n",virt_mem[n_addr]);
}

int cpu_clock = 20; //20hz

#define cur_com virt_mem[exec_ptr]

void execute(){
    unsigned int run_per_sec = 1000000 / cpu_clock  > 0 ? 1000000 / cpu_clock : 0;
    unsigned int exec_count=0;
    for (int halt=0;!halt;exec_ptr++)
    {
        exec_count++;
        cpu_clock == -1?:usleep(run_per_sec);
        
        if(cur_com == STORAGE){
            logger("Storage command found\n");
            execute_mode = execute_mode!=EXECUTE_STORAGE?EXECUTE_STORAGE:EXECUTE_NORMAL;
            logger("Executed count: %d\n\n", exec_count);
            continue;
        }

        if(execute_mode == EXECUTE_STORAGE){
            logger("Skipped %08x\n",cur_com);
            logger("Executed count: %d\n\n", exec_count);
            continue;
        }

        switch (cur_com)
        {
        case ADD:
            add();
            break;
        
        case SUB:
            sub();
            break;
        
        case XADD:
            xadd();
            break;
        
        case XSUB:
            xsub();
            break;

        case LAND:
            land();
            break;

        case LOR:
            lor();
            break;

        case LNOT:
            lnot();
            break;

        case MOV:
            mov();
            break;

        case COPY:
            copy();
            break;

        case GOTO:
            _goto();
            break;

        case GETA:
            geta();
            break;

        case MORE:
            more();
            break;

        case LESS:
            less();
            break;

        case EQUA:
            equa();
            break;

        case LM:
            lm();
            break;

        case RM:
            rm();
            break;

        case PUTC:
            _putc();
            break;

        case PUTN:
            putn();
            break;

        case PUTH:
            puth();
            break;

        case GETC:
            _getc();
            break;

        case GETN:
            getn();
            break;

        case GETH:
            geth();
            break;

        case EXIT:
            logger("Exit()\n");
            halt = 1;
            break;

        case AND:
            and();
            break;

        case OR:
            or();
            break;

        case XOR:
            xor();
            break;
        
        case NOT:
            not();
            break;

        case SHL:
            shl();
            break;

        case SHR:
            shr();
            break;

        case VOID:
            logger("Execute Failed: Void Byte Found!\n");
            printf("Execute Failed: Void Byte Found!\n");
            halt = 1;
            continue;

        default:
            logger("Unknown %08x\n", cur_com);
            break;
        }
        logger("Executed count: %d\n\n", exec_count);
    }
}


int main(int argc, char** argv){
    if(init_virt_memory()){
        printf("Could not allocate memory\n");
        return 1;
    }
    struct OptData opt_data = parse_options(argc, argv);
    if(opt_data.exit){
        return 0;
    };
    char* input_filename = opt_data.input_file;
    if(!opt_data.non_log_file){
        log_file_name = opt_data.log_file;
        log_file = fopen(log_file_name,"w");
        if(log_file == NULL){
            printf("Failed to open log file: %s\n",log_file_name);
            exit(1);
        }
    }
    logger("input file: %s\n",input_filename);
    read_coms_to_mem(input_filename);
    cpu_clock = opt_data.cpu_clock;
    execute();
    free(virt_mem);
    return 0;
}