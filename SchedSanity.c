// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"
#define MEDIUM 40
#define LARGE  100
#define PROC_NUM 10



int fib(int x)
{
    if(x==0)
    {
        return 0;
    }
    if(x==1)
    {
        return 1;
    }

    return (fib(x-1)+fib(x-2));
}

int rtime=0, wtime=0, iotime=0; 

int calc_large(){
    set_priority(1);/*
    int i,dummy=0;
    for(i=0; i<LARGE; i++){
        dummy = dummy + 12;
        int j=1;
        long mul=1;
        for(;j<2334;j++)
        {
            mul=mul*j;
            int k=1;
            for(;k<1000;k++){
                dummy++;
                asm("");
            }
        }
    }*/
        fib(40);
    //printf(1,"%d",r);
    exit();
}

int calc_medium(){
    set_priority(1);
   /* int i,dummy=0;
    for(i=0; i<MEDIUM; i++){
        dummy = dummy + 12;
        int j=1;
        long mul=1;
        for(;j<i;j++)
        {
            mul=mul*j;
            int k=1;
            for(;k<1000;k++){
                dummy++;
            }
        }
    }*/
        fib(35);
     //  printf(1,"%d",r);
    exit();
}


int calc_io_large(){
    set_priority(3);
    int i;
    for(i=0; i<LARGE; i++){
        //printf(1,"run a large sized loop\n");
        sleep(1);
    }
    exit();
}
        

int calc_io_medium(){
    set_priority(3);
    int i;
    for(i=0; i<MEDIUM; i++){
        //printf(1,"These program run in medium sized loop\n");
        sleep(1);
    }
    exit();
}

        


int main(void){
    int pids [40]={0};
    int i=0;
    int wtimes [40][1]={0};
    int rtimes  [40][1]={0};
    int iotimes [40][1]={0};
    
    for(;i<40;i++){
        if(i%4==0){
            int tmp=fork();
            if(tmp!=0)
            pids[i]=tmp;
            if(pids[i]<0){
                printf(2,"fork failed\n");
            }
            if(pids[i]==0){
                calc_medium();
            }
            else{
                continue;
            }
        }
        if(i%4==1){
            int tmp=fork();
            if(tmp!=0)
            pids[i]=tmp;
            if(pids[i]<0){
                printf(2,"fork failed\n");
            }
            if(pids[i]==0){
                calc_large();
            }
            else{
                continue;
            }
        }
         if(i%4==2){
            int tmp=fork();
            if(tmp!=0)
            pids[i]=tmp;
            if(pids[i]<0){
                printf(2,"fork failed\n");
            }
            if(pids[i]==0){
                calc_io_medium();
            }
            else{
                continue;
            }
        }
         if(i%4==3)
        {
            int tmp=fork();
            if(tmp!=0)
            pids[i]=tmp;
            if(pids[i]<0){
                printf(2,"fork failed\n");
            }
            if(pids[i]==0){
                calc_io_large();
            }
            else{
                continue;
            }
        }
    }
//     int wait2(int pid,int* wtime,int* rtime,int* iotime)

    for(i=0;i<40;i++){
        wait2(pids[i],wtimes[i],rtimes[i],iotimes[i]);
    }

    long sumio1=0, sumrtime1=0, sumwtime1=0;
    long sumio2=0, sumrtime2=0, sumwtime2=0;
    long sumio3=0, sumrtime3=0, sumwtime3=0;
    long sumio4=0, sumrtime4=0, sumwtime4=0;

    for (i=0;i<40;i++){
        if(i%4==0){
        sumio1+=iotimes[i][0];
        sumrtime1+=rtimes[i][0];
        sumwtime1+=wtimes[i][0];
    }
        if(i%4==1){
        sumio2+=iotimes[i][0];
        sumrtime2+=rtimes[i][0];
        sumwtime2+=wtimes[i][0];
    }
        if(i%4==2){
        sumio3+=iotimes[i][0];
        sumrtime3+=rtimes[i][0];
        sumwtime3+=wtimes[i][0];
    }
        if(i%4==3){
        sumio4+=iotimes[i][0];
        sumrtime4+=rtimes[i][0];
        sumwtime4+=wtimes[i][0];
    }
    }

    printf(1,"\nResults For Meduim Calculations\n");
    printf(1,"Avg iotime %d \t Avg rtime %d \t Avg wtime %d \n\n",(sumio1/PROC_NUM),(sumrtime1/PROC_NUM),(sumwtime1/PROC_NUM));

    printf(1,"Results For Large Calculations\n");
    printf(1,"Avg iotime %d \t Avg rtime %d \t Avg wtime %d \n\n",(sumio2/PROC_NUM),(sumrtime2/PROC_NUM),(sumwtime2/PROC_NUM));

    printf(1,"Results For Meduim IO Calculations\n");
    printf(1,"Avg iotime %d \t Avg rtime %d \t Avg wtime %d \n\n",(sumio3/PROC_NUM),(sumrtime3/PROC_NUM),(sumwtime3/PROC_NUM));

    printf(1,"Results For Large IO Calculations\n");
    printf(1,"Avg iotime %d \t Avg rtime %d \t Avg wtime %d \n\n",(sumio4/PROC_NUM),(sumrtime4/PROC_NUM),(sumwtime4/PROC_NUM));

    exit();
}


//40 process defualt scheduler average iotime147 average rtime156,average wtime973 