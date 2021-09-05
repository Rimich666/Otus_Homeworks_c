#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h> 

#define SOI 65496
#define EOI 65497 
#define IDS 65535
#define SOS 65498

#define LFH 0x04034b50
#define EOCDR 0x06054b50
#define EOCDR__SIZE 22
#define CDHF_SIZE 46

static size_t size_file = 0;
static size_t end_jpeg = 0;

u_int16_t getshort(FILE* pfile, int LE);
u_int32_t get_int(FILE* pfile, int LE);
void checkfile(char* filename, FILE* pfile);
void findendjpeg(FILE* pfile);
uint findendsegment(FILE* pfile);
void readZIP(FILE* pfile);
void findEOCDR(FILE* pfile);
size_t read_header_file(FILE* pfile);

int main(int argc, char* argv[])
{   
    printf("Чёт должно получиться\n");
    printf("argc = %d\n",argc);
    for(int i=0;i<argc;i++)
    {
        printf("argv[%d] = %s\n", i, argv[i]);
    };
    char filename[strlen(argv[1])];
    strcpy(filename,argv[1]);
    printf("Имя файла: %s\t end\n",filename);
    FILE* pfile = fopen(filename, "r");
    checkfile(filename, pfile);
    fclose(pfile);
    return 0;
}

void checkfile(char* filename, FILE* pfile)
{
    printf("%d:  Дескриптор файла: %d\n", __LINE__ ,pfile->_fileno);
    fseek(pfile,0,SEEK_SET);
    u_int16_t signat = getshort(pfile, 0);
    printf("Сигнатура начала: %x \n", signat); 
    if (signat != SOI)
    {
        printf("%s - не jpeg\n",filename);
        return;
    };
    fseek(pfile,0,SEEK_END);
    size_file = ftell(pfile);
    fseek(pfile,-2,SEEK_END);
    signat = getshort(pfile, 0);
    if (signat == EOI)
    {
        printf("%s - \x1b[32mне содержит архива\x1b[0m\n",filename);
        return;
    };
    findendjpeg(pfile);


    fseek(pfile,-2,SEEK_CUR);
    printf("Сигнатура конца JPEG: %x \n", getshort(pfile, 0)); 
    fseek(pfile,2,SEEK_CUR);
    end_jpeg = ftell(pfile);
    u_int32_t signat32 = get_int(pfile, 1);
    printf("Сигнатура ZIP?: %x\n", signat32);
    
    if (signat32 == LFH) readZIP(pfile);
}

size_t read_header_file(FILE* pfile)
{
    u_int32_t signat32 = get_int(pfile, 1);
    printf("%d: Сигнатура HF?: %x\n", __LINE__,signat32);
    fseek(pfile, 28, SEEK_CUR);
    u_int16_t name_len = getshort(pfile, 1);
    fseek(pfile, 2, SEEK_CUR);
    u_int16_t extra_len = getshort(pfile, 1);
    fseek(pfile, 2, SEEK_CUR);
    u_int16_t comment_len = getshort(pfile, 1);
    fseek(pfile, 14, SEEK_CUR);
    printf("name_len: %x, extra_len: %d comment_len: %d\n",name_len, extra_len, comment_len);
    
    u_int8_t* buf;
    buf = (u_int8_t*)malloc(name_len * sizeof(u_int8_t));
    void* ptrvoid = (void*)buf;
    fread(ptrvoid, name_len, 1, pfile);
    printf("%s\n", (char*)ptrvoid);
    free(buf);

    return name_len + extra_len + comment_len + CDHF_SIZE;
}

void readZIP(FILE* pfile)
{
    findEOCDR(pfile); 
    printf("%d: Сигнатура конца ZIP: %x\n", __LINE__, get_int(pfile, 1));
    int cd_size_offset = 4 + 2 * 4;
    fseek(pfile, cd_size_offset, SEEK_CUR);
    size_t cd_size = get_int(pfile, 1);
    size_t headers_size = 0;
    fseek(pfile,4,SEEK_CUR);
    size_t cd_offset = get_int(pfile, 1);
    printf("%d: cd_size: %d\n", __LINE__, cd_size);
    printf("%d: cd_offset: %x\n", __LINE__, (end_jpeg + cd_offset));
    do
    {
        fseek(pfile, end_jpeg + cd_offset + headers_size, SEEK_SET);
        headers_size += read_header_file(pfile);
        printf("%ld из %ld\n", headers_size, cd_size);
    }
    while (headers_size < cd_size);
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

void findendjpeg(FILE* pfile)
{
    fseek(pfile,2,SEEK_SET);
    findendsegment(pfile);
    printf("Конец jpeg Point: %ld\n",ftell(pfile));
        
}

uint findendsegment(FILE* pfile)
{
    static int counter = 0;
    u_int16_t sizeseg;
    u_int16_t signat;
    signat = getshort(pfile, 0);   
    printf("Сигнатура сегмента: %x\n",signat); 
    counter+=1;
    if (counter==13)return 1;
    if (signat==IDS)
    {
        fseek(pfile,1,SEEK_CUR);
        printf("Лишняя Ф-ка Point: %ld\n",ftell(pfile));
        return findendsegment(pfile);
    }
    else if (signat==EOI)
    {
//        fseek(pfile,2,SEEK_CUR);
        printf("Конец картинки Point: %ld\n",ftell(pfile));      
        return 0;
    }
    else if (signat==SOS)
    {
        fseek(pfile,2,SEEK_CUR);
        printf("Начало картинки Point: %ld\n",ftell(pfile));
        while (ftell(pfile)<size_file-1)
        {
            fseek(pfile,1,SEEK_CUR);
            signat = getshort(pfile, 0);
            if (signat==EOI)
            {
                fseek(pfile,2,SEEK_CUR);
                printf("Конец картинки Point: %ld\n",ftell(pfile));      
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
        printf("Пустой сегмент Point: %ld\n",ftell(pfile));      
        return findendsegment(pfile);   
    }  
    fseek(pfile,sizeseg + 2,SEEK_CUR);
    printf("Размер сегмента: %d\n",sizeseg);      
    return findendsegment(pfile);
}
