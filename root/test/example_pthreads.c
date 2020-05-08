#include <stdio.h>
#include <pthread.h>
        
/*thread function definition*/
void* threadFunction(void* args)
{
    int idx = 0;
    while( idx < 3)
    {
        idx++;
        printf("I am threadFunction.\n");
    }
}
int main()
{
    /*creating thread id*/
    pthread_t id;
    int ret;
    
    /*creating thread*/
    //pthread_create(&id,NULL,&threadFunction,NULL);
    ret=pthread_create(&id,NULL,&threadFunction,NULL);
    if(ret==0){
        printf("Thread created successfully:%x\n", id);
    }
    else{
        printf("Thread not created.\n");
        return 0; /*return from main*/
    }
    
    int idx = 0;
    while(idx < 5)
    {
        idx++;
        printf("I am main function.\n");      
    }
    
    return 0;
}
