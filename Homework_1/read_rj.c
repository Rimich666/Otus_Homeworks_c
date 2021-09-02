#include <stdio.h>
#include <stdlib.h>
#include<string.h>

#define SOI 65496
#define EOI 65497 
#define IDS 65535
#define SOS 65498


uint size_file = 0;

u_int16_t getshort(char* buf);
void checkfile(char* filename, FILE* pfile);
void findendjpeg(FILE* pfile);
uint findendsegment(FILE* pfile);

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
    printf("Дескриптор файла: %d\n",pfile->_fileno);
    fseek(pfile,0,SEEK_SET);
    char buf[3]="00\0";
    void* ptrvoid = (void*)buf;
    size_t res = fread(ptrvoid, 2, 1, pfile);
    if (res != 1) 
    {
        printf("Ошибка чтения файла");
        return; 
    }
    u_int16_t signat = getshort(buf);
    if (signat != SOI)
    {
        printf("%s - не jpeg\n",filename);
        return;
    };
    fseek(pfile,0,SEEK_END);

    fseek(pfile,-2,SEEK_END);
    res = fread(ptrvoid, 2, 1, pfile);
    signat = getshort(buf);
    if (signat == EOI)
    {
        printf("%s - \x1b[32mне содержит архива\x1b[0m\n",filename);
        return;
    };
    findendjpeg(pfile);


//    printf("Размер size_t %ld\n", sizeof(size_t));
//    printf("Ура считан символ %d\n", (u_int16_t)buf[0]);
    fseek(pfile,-2,SEEK_CUR);
    res = fread(ptrvoid, 2, 1, pfile);
    printf("%x %x\n",(u_int8_t)buf[0], (u_int8_t)buf[1]); 
    fseek(pfile,2,SEEK_CUR);

    
}

u_int16_t getshort(char* buf)
{
    u_int16_t shrt = (u_int8_t)buf[0];
    shrt <<=8;
    shrt += (u_int8_t)buf[1];
    return shrt;
}

void findendjpeg(FILE* pfile)
{
    fseek(pfile,2,SEEK_SET);
    findendsegment(pfile);
    printf("Конец jpeg Point %ld\n",ftell(pfile));
        
}

uint findendsegment(FILE* pfile)
{
    static int counter = 0;
    char buf[3]="00\0";
    void* ptrvoid = (void*)buf;
    u_int16_t sizeseg;
    u_int16_t signat;
    printf("До чтения Point %ld\n",ftell(pfile));
    size_t res = fread(ptrvoid, 2, 1, pfile);
    fseek(pfile,-2,SEEK_CUR);
    signat = getshort(buf);   
    printf("На входе Point %ld\n",ftell(pfile));
    printf("%x %x\n",(u_int8_t)buf[0], (u_int8_t)buf[1]); 
//   printf("%x\n",(u_int8_t)buf[1]);
    printf("%d\n",signat); 
    counter+=1;
    if (counter==13)return 1;
    if (signat==IDS)
    {
        fseek(pfile,1,SEEK_CUR);
        printf("Лишняя Ф-ка Point %ld\n",ftell(pfile));
        return findendsegment(pfile);
    }
    else if (signat==EOI)
    {
//        fseek(pfile,2,SEEK_CUR);
        printf("Конец картинки Point %ld\n",ftell(pfile));      
        return 0;
    }
    else if (signat==SOS)
    {
        fseek(pfile,2,SEEK_CUR);
        printf("Начало картинки Point %ld\n",ftell(pfile));
        while (ftell(pfile)<size_file-1)
        {
            fseek(pfile,1,SEEK_CUR);
            res = fread(ptrvoid, 2, 1, pfile);
            signat = getshort(buf);
            if (signat==EOI)
            {
//                fseek(pfile,2,SEEK_CUR);
                printf("Конец картинки Point %ld\n",ftell(pfile));      
                return 0;
            }   
        }
                  
    }
    fseek(pfile,2,SEEK_CUR);

    res = fread(ptrvoid, 2, 1, pfile);
    fseek(pfile,-2,SEEK_CUR);
    if ((u_int8_t)buf[0]==IDS) 
    {
        printf("Пустой сегмент Point %ld\n",ftell(pfile));      
        return findendsegment(pfile);   
    }  
    sizeseg = getshort(buf);
    fseek(pfile,sizeseg,SEEK_CUR);
    printf("Размер сегмента %d\n",sizeseg);      
    return findendsegment(pfile);
}