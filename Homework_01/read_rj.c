#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h> 
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>

#define SOI 65496
#define EOI 65497 
#define IDS 65535
#define SOS 65498

#define LFH 0x04034b50
#define EOCDR 0x06054b50
#define EOCDR__SIZE 22
#define CDHF_SIZE 46

//#define DEBUG 
#ifdef DEBUG
    #define DPRINT(str,...) printf("%d: ", __LINE__);\
                            printf(str, __VA_ARGS__);\
                            printf("\n");
#else
    #define DPRINT(str, ...)
#endif

static size_t size_file = 0;
static size_t end_jpeg = 0;

u_int16_t getshort(FILE* pfile, int LE);
u_int32_t get_int(FILE* pfile, int LE);
void checkfile(char* fname, FILE* pfile);
int findendjpeg(FILE* pfile);
uint findendsegment(FILE* pfile);
void readZIP(FILE* pfile);
void findEOCDR(FILE* pfile);
size_t read_header_file(FILE* pfile);
char* curdir(char *fname);
void print_line(char *node);
int numchar(char *str, u_int8_t ch);

int main(int argc, char* argv[])
{   
    DPRINT("%s", "Чёт должно получиться");
    DPRINT("argc = %d",argc);
    for(int i=0;i<argc;i++)
    {
        DPRINT("argv[%d] = %s", i, argv[i]);
    };
    FILE* pfile;
    DIR *pdir;
    struct dirent *pent;
    char *cur_dir = curdir(argv[0]);
    char *fname;
    char dfname[PATH_MAX];
    int lenfname; 
    if (argc==1){fname = cur_dir;}
    else if (argc==2){fname = argv[1];}
    else 
    {
        printf("Слишком много аргументов.");
        return 0;
    }
    struct stat buff;
    //struct stat bufff;
    stat(fname, &buff);
    DPRINT("Номер inode файла: %ld", buff.st_ino);
    if (buff.st_ino == 0)
    {
        printf("\x1b[31m\x1b[1mНет такого файла:\x1b[0m %s\n", fname);
        free(cur_dir);        
        return 0;
    }

    if ((buff.st_mode & S_IFMT) == S_IFDIR)
    {
        DPRINT("%s это дирректория", fname);
        pdir = opendir(fname);
        lenfname = strlen(fname);
        if (pdir != NULL)
        {
            while(pent = readdir(pdir))
            {
                if (pent->d_name[0] != '.')
                {
                    strcpy(dfname, fname);
                    if (fname[lenfname-1] != '/')
                    {
                        dfname[lenfname] = '/';
                        dfname[lenfname+1] = '\0';
                    }
                    strcat(dfname, pent->d_name);
                    stat(dfname, &buff);
                    if((buff.st_mode & S_IFMT) == S_IFREG)
                    {
                        printf("\x1b[32mПроверяемый файл: \x1b[4m\x1b[33m%s\x1b[0m\n", dfname);
                        pfile = fopen(dfname, "r");
                        checkfile(dfname, pfile);
                        fclose(pfile);
                    }            
                }
            }
            closedir(pdir);
        }
        
    }
    else if((buff.st_mode & S_IFMT) == S_IFREG)
    {
        DPRINT("%s это обычный файл", fname);
        pfile = fopen(fname, "r");
        checkfile(fname, pfile);
        fclose(pfile);        
    }
    else
    {
        printf("%s это неведома зверушка\n", fname);
    }
    
    
    DPRINT("Имя файла: %s\t end", fname);
    free(cur_dir);
    return 0;
}

char* curdir(char *fname)
{
    char *pcd = (char*)malloc(strlen(fname));
    strcpy(pcd, fname);
    char *plp = strrchr(pcd, '/');
    int ppp = plp - pcd + 1;
    pcd[ppp] = '\0';
    pcd = (char*)realloc(pcd, ppp + 4);
    pcd = strcat(pcd, "src/");
    return pcd;
}

void checkfile(char* fname, FILE* pfile)
{
    DPRINT("Дескриптор файла: %d", pfile->_fileno);
    fseek(pfile,0,SEEK_SET);
    u_int16_t signat = getshort(pfile, 0);
    DPRINT("Сигнатура начала: %x ", signat); 
    if (signat != SOI)
    {
        printf("\x1b[36m%s\x1b[32m - не jpeg\x1b[0m\n",fname);
        return;
    };
    fseek(pfile,0,SEEK_END);
    size_file = ftell(pfile);
    fseek(pfile,-2,SEEK_END);
    signat = getshort(pfile, 0);
    if (signat == EOI)
    {
        printf("\x1b[36m%s - \x1b[32mне содержит архива\x1b[0m\n",fname);
        return;
    };
    int res = findendjpeg(pfile);
    if (res == 1)
    {
        printf("\x1b[31m\x1b[32mЧто то пошло не так.\x1b[0m\n");
    }

    fseek(pfile,-2,SEEK_CUR);
    DPRINT("Сигнатура конца JPEG: %x ", getshort(pfile, 0)); 
    fseek(pfile,2,SEEK_CUR);
    end_jpeg = ftell(pfile);
    u_int32_t signat32 = get_int(pfile, 1);
    DPRINT("Сигнатура ZIP?: %x", signat32);
    
    if (signat32 == LFH) 
    {
        printf("\x1b[36m%s - \x1b[32m содержит архив ZIP\x1b[0m\n",fname);
        readZIP(pfile);
        return;
    }
    printf("\x1b[36m%s - \x1b[32m содержит нечто ...\x1b[0m\n",fname);    
}

size_t read_header_file(FILE* pfile)
{
    u_int32_t signat32 = get_int(pfile, 1);
    fseek(pfile, 28, SEEK_CUR);
    u_int16_t name_len = getshort(pfile, 1);
    fseek(pfile, 2, SEEK_CUR);
    u_int16_t extra_len = getshort(pfile, 1);
    fseek(pfile, 2, SEEK_CUR);
    u_int16_t comment_len = getshort(pfile, 1);
    fseek(pfile, 14, SEEK_CUR);
    
    u_int8_t* buf;
    buf = (u_int8_t*)malloc(name_len * sizeof(u_int8_t));
    void* ptrvoid = (void*)buf;
    fread(ptrvoid, name_len, 1, pfile);
    print_line((char*)ptrvoid);
    free(buf);

    return name_len + extra_len + comment_len + CDHF_SIZE;
}

int numchar(char *str, u_int8_t ch)
{
    int pos = 0;
    int num = 0;
    do
    {
        if(str[pos] == '/'){num += 1;} 
        pos += 1;
    } while (str[pos] != '\0');
    return num;
}

void print_line(char *node)
{
    static char prev[256] = "";
    static int prevlevel = 0;
    static char color[16];
    char *prostrel = "│  ";
    char *ugol = "└─ ";
    char *vilka = "├─ "; 
    int level = numchar(node, '/');
    char *clr;
    
    if (node[strlen(node)-1]!='/')
    {
        level += 1;
        clr = "\x1b[36m\x1b[1m";
    }
    else
    {
        node[strlen(node)-1] = '\0';
        clr = "\x1b[34m\x1b[1m";
    }
    if(node=="")level = 0;
    if (prevlevel>1)
    {
        for (int i=2; i<prevlevel; i++){printf("%s", prostrel);}
        if (level<prevlevel){printf("%s", ugol);}
        else{printf("%s", vilka);}
    }
    if (prevlevel>0)
    {
        printf("%s", color);
        printf("%s \x1b[0m\n", prev);
    }
    
    prevlevel = level;
    strcpy(color, clr);
    char * nm = strrchr(node, '/');
    if (nm != NULL){strcpy(prev, &nm[1]);}
    else{strcpy(prev, node);}
}

void readZIP(FILE* pfile)
{
    findEOCDR(pfile); 
    DPRINT("Сигнатура конца ZIP: %x", get_int(pfile, 1));
    int cd_size_offset = 4 + 2 * 4;
    fseek(pfile, cd_size_offset, SEEK_CUR);
    size_t cd_size = get_int(pfile, 1);
    size_t headers_size = 0;
    fseek(pfile,4,SEEK_CUR);
    size_t cd_offset = get_int(pfile, 1);
    DPRINT("cd_size: %lu", cd_size);
    DPRINT("cd_offset: %lu", (end_jpeg + cd_offset));
    do
    {
        fseek(pfile, end_jpeg + cd_offset + headers_size, SEEK_SET);
        headers_size += read_header_file(pfile);
    }
    while (headers_size < cd_size);
    print_line("");
}


void findEOCDR(FILE* pfile)
{
    size_t comment_len;
    size_t minus_p = size_file - end_jpeg - EOCDR__SIZE;
    size_t limit = minus_p < __UINT16_MAX__ ? minus_p : __UINT16_MAX__;
    u_int32_t signat =0;
    int p;
    fseek(pfile, -minus_p-EOCDR__SIZE, SEEK_END);
    for(comment_len=0; comment_len<limit; comment_len++)
    {
        p = - comment_len - EOCDR__SIZE;
        fseek(pfile, p, SEEK_END);
        signat = get_int(pfile, 1); 
        if(signat == EOCDR) break; 
    }
}


u_int32_t get_int(FILE* pfile, int LE)
{
    char buf[5]="0000\0";
    void* ptrvoid = (void*)buf;
    size_t res = fread(ptrvoid, 4, 1, pfile);
    u_int32_t int32 = 0;
    int j;
    
    for(int i=0; i<4; i++)
    {
        j = LE == 0 ? i: 3 - i; 
        int32 += (u_int8_t)buf[j]<<((3-i)*8);
    }
    fseek(pfile, -4, SEEK_CUR);
    return int32;
}

u_int16_t getshort(FILE* pfile, int LE)
{
    char buf[3]="00\0";
    void* ptrvoid = (void*)buf;
    int j = LE == 0 ? 1: 0; 
    size_t res = fread(ptrvoid, 2, 1, pfile);
    u_int16_t shrt = (u_int8_t)buf[LE];
    shrt <<=8;
    shrt += (u_int8_t)buf[j];
    fseek(pfile, -2, SEEK_CUR);
    return shrt;
}

int findendjpeg(FILE* pfile)
{
    fseek(pfile,2,SEEK_SET);
    int res = findendsegment(pfile);
    DPRINT("Конец jpeg Point: %ld",ftell(pfile));
    return res;    
}

uint findendsegment(FILE* pfile)
{
    static int counter = 0;
    u_int16_t sizeseg;
    u_int16_t signat;
    signat = getshort(pfile, 0);   
    DPRINT("Сигнатура сегмента: %x",signat); 
    counter+=1;
    if (counter==13)return 1;
    if (signat==IDS)
    {
        fseek(pfile,1,SEEK_CUR);
        DPRINT("Лишняя Ф-ка Point: %ld",ftell(pfile));
        return findendsegment(pfile);
    }
    else if (signat==EOI)
    {
        DPRINT("Конец картинки Point: %ld",ftell(pfile));      
        counter = 0;
        return 0;
    }
    else if (signat==SOS)
    {
        fseek(pfile,2,SEEK_CUR);
        DPRINT("Начало картинки Point: %ld",ftell(pfile));
        while (ftell(pfile)<size_file-1)
        {
            fseek(pfile,1,SEEK_CUR);
            signat = getshort(pfile, 0);
            if (signat==EOI)
            {
                fseek(pfile,2,SEEK_CUR);
                DPRINT("Конец картинки Point: %ld",ftell(pfile));      
                counter = 0;
                return 0;
            }   
        }
                  
    }
    fseek(pfile,2,SEEK_CUR);

    sizeseg = getshort(pfile, 0);
 //   res = fread(ptrvoid, 2, 1, pfile);
    fseek(pfile,-2,SEEK_CUR);
    if ((u_int8_t)(sizeseg>>8)==IDS) 
    {
        DPRINT("Пустой сегмент Point: %ld",ftell(pfile));      
        return findendsegment(pfile);   
    }  
    fseek(pfile,sizeseg + 2,SEEK_CUR);
    DPRINT("Размер сегмента: %d",sizeseg);      
    return findendsegment(pfile);
}
