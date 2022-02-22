#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#include <termios.h>	//our favorite library


static int pressCount=0;
static struct termios old, new;

void initTermios(int echo)	//terminal settings
{
    tcgetattr(0, &old); //grab old terminal i/o settings
    new = old; //make new settings same as old settings
    new.c_lflag &= ~ICANON; //disable buffered i/o
  //  new.c_lflag &= echo ? ECHO : ~ECHO; //set echo mode --->this line changed with the line underneath it
    new.c_cflag= ~ECHO;	//this is the key part, if we dont do this we cant read the up and down arrow keys.
    tcsetattr(0, TCSANOW, &new); //apply terminal io settings
}

void resetTermios(void)
{
    tcsetattr(0, TCSANOW, &old);
}

char getch_(int echo)
{
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

char getch(void)
{
    return getch_(0);
}
//-----------------------------------------------------getch--------------------------------------

struct inputLines{
//int position;
char *line;
struct inputLines *before;
struct inputLines *next;
};

struct inputLines *firstLine;
//struct inputLines *currentLine; //needless
//struct inputLines *lastLine;

void cpStr(char source[], char dest[]){	//copies string to another string
    int length=strlen(source);
    for(int i=0;i<length;i++){
        dest[i]=source[i];
    }
}
bool anyForegroundProcess = false;
pid_t currentForegroundPid;



void shiftString(char *pString[], int delete){//deletes a value from string array by shifting
    int i=0;
    while(pString[i]!=NULL){
        if(i>=delete){
            pString[i]=pString[i+1];
        }
        i++;
    }
}

void checkInputOutputChoice(char *pString[]) {
//    printf("--------------------------------------------------\nRedirection Part\n");
    int i = 0;
    char letter[2];
    char *location =malloc(127* sizeof(char)) ;

    int fperr=0;
    int fpwrite = 0;
    int fpread = 0;

//    int *deleteLocations; //hakana bakıp silmem gerekiyor mu stringin < > 2> içeren kısımlarını diye sorulacak

#define CREATE_FlAGS (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

    //checks < > 2> >> 2>> arguments in the input line.
    while (pString[i] != NULL) {
        //location[0]='\0';
        memcpy(letter,pString[i],1);
        letter[1]='\0';

        int lineCount=strlen(location);
        for(int k=0;k<lineCount;k++)
            location[k]='\0';

        //output
        if(strcmp(letter,">")==0){
            memcpy(letter,&pString[i][1],1);
            letter[1]='\0';
            if(strcmp(letter,">")==0){//cheks if its >>out.txt or >> out.txt
                if(strlen(pString[i])<=2){//devam etmiyorsa sonra stringe bakıyor
                    memcpy(location, pString[i+1], strlen(pString[i+1]));
                    shiftString(pString,i);		//deleting ith string of pString arr
                    shiftString(pString,i);		//deleting ith string of pString arr
                }
                else {// >>out.txt
                    memcpy(location, &pString[i][2], strlen(pString[i])-2);
                    shiftString(pString,i);		//deleting ith string of pString arr
                }
                location[127]='\0';
                fpwrite = open(location, O_APPEND|O_WRONLY|O_CREAT,CREATE_FlAGS);//creates or appends not sure about creating part if it does not exist
                continue;
            }else{//no append
                if(strlen(pString[i])<=1){//checks if it is > out.txt or >out.txt
                    int m=strlen(pString[i+1]);
                    memcpy(location,pString[i+1],m);
                    shiftString(pString,i);		//deleting ith string of pString arr
                    shiftString(pString,i);		//deleting ith string of pString arr
                }else {// if it is >out.txt
                    memcpy(location, &pString[i][1], strlen(pString[i])-1);
                    shiftString(pString,i);		//deleting ith string of pString arr
                }
                location[127]='\0';
                fpwrite = open(location, O_CREAT|O_WRONLY|O_TRUNC,CREATE_FlAGS);//opening file
                continue;
            }
        }
        //input redirection
        if(strcmp(letter,"<")==0){//taking input
            if(strlen(pString[i])<=1) {	//checks if it is <in.txt or < in.txt
                memcpy(location,pString[i+1],strlen(pString[i+1]));
                shiftString(pString,i);		//deleting ith string of pString arr
                shiftString(pString,i);		//deleting ith string of pString arr
            }else{	// <in.txt
                memcpy(location,&pString[i][1],strlen(pString[i])-1);
                shiftString(pString,i);
            }
            location[127]='\0';

            fpread = open(location, O_RDONLY,CREATE_FlAGS);// opens the file at location
            continue;
        }
        //error redirection
        if(strcmp(letter,"2")==0){
            memcpy(letter,&pString[i][1],1);
            letter[1]='\0';
            if(strcmp(letter,">")==0){//checks if its only 2 or 2>
                memcpy(letter,&pString[i][2],1);
                letter[1]='\0';
                if(strcmp(letter,">")==0){//if is appends
                    if(strlen(pString[i])<=3){//checks if it is next to 2>> or not
                        memcpy(location, pString[i+1], strlen(pString[i+1]));
                        shiftString(pString,i);	//deleting ith string of pString arr
                        shiftString(pString,i);	//deleting ith string of pString arr
                    }
                    else {//if not, then reads the next string
                        memcpy(location, &pString[i][3], strlen(pString[i])-3);
                        shiftString(pString,i);	//deleting ith string of pString arr
                    }
                    location[127]='\0';
                    fperr = open(location, O_APPEND|O_WRONLY|O_CREAT,CREATE_FlAGS);
                    continue;
                }else{//no append
                    if(strlen(pString[i])<=2) { //cheks if it is only 2> or 2>out.txt
                        memcpy(location, pString[i+1], strlen(pString[i+1]));
                        shiftString(pString,i);	//deleting ith string of pString arr
                        shiftString(pString,i);	//deleting ith string of pString arr
                    }
                    else {//if it is 2>out.txt
                        memcpy(location, &pString[i][2], strlen(pString[i])-2);
                        shiftString(pString,i);	//deleting ith string of pString arr
                    }
                    location[127]='\0';
                    fperr = open(location, O_CREAT|O_WRONLY|O_TRUNC,CREATE_FlAGS);//opens err file at location
                    continue;
                }
            }
        }

        i++;
    }

    //error checkings and operations
    if(fpread != 0){
        if (fpread == -1) {
            perror("error at opening input file");	//checks if there is any error at opening file
        } else
        if (dup2(fpread, STDIN_FILENO) == -1) {	//duplicating
            perror("error at changing STDIN");
        } else
        if (close(fpread) == -1) {			//closing
            perror("error at closing read file");
        }
    }
    if (fpwrite != 0) {
        if (fpwrite == -1) {//checks if there is any error at opening file
            perror("error at creating output file");
        } else
        if (dup2(fpwrite, STDOUT_FILENO) == -1) {	//duplicating
            perror("error at changing STDOUT");
        } else
        if (close(fpwrite) == -1) {			//closing
            perror("error at closing write file");
        }
    }
    if (fperr != 0){
        if (fperr == -1) {//checks if there is any error at opening file
            perror("error at creating output file");
        } else
        if (dup2(fperr, STDERR_FILENO) == -1) {	//duplicating
            perror("error at changing STDERR");
        } else
        if (close(fperr) == -1) {			//closing
            perror("error at closing error file");
        }
    }
}



/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */
void setup(char inputBuffer[], char *args[],int *background){
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;
    char c='a'; //nothing important

    struct inputLines *newLine;
    struct inputLines *currentLine;
    if(firstLine==NULL){    //checks if the first line started or not
        //printf("pls");
        firstLine=malloc(sizeof(struct inputLines)+80);
        newLine=firstLine;
        newLine->next=NULL;
        newLine->before=NULL;
        newLine->line=(char *)malloc(120* sizeof(char));
        currentLine=newLine;
    }else{//if the first line is started
        currentLine=firstLine;
        while(currentLine->next!=NULL){// son elemanını buluyor
            currentLine=currentLine->next;
        }
        newLine=malloc(sizeof(struct inputLines)+80);//başlatıp
        newLine->next=NULL;
        newLine->before=currentLine;
        newLine->line=(char *)malloc(120* sizeof(char));
        currentLine->next=newLine;
        currentLine=currentLine->next;//add its into the end of linked list

    }


    printf("\r                                                                                  ");
    printf("\rmyshell: ");

    char *line=malloc(80*sizeof(char));
    //line[0]='\0';
    /* read what the user enters on the command line */
    for(int m=0;c!='\n';m++){
        c=getch();

        switch(c){
            case 'A':
                if(currentLine->before!=NULL){
                    currentLine=currentLine->before; 	//moves currentLine to one line before

                    int lineCount=strlen(line);	//resets the line in order to upload the other one
                    for(int k=0;k<lineCount;k++)
                        line[k]='\0';

                    cpStr(currentLine->line,line);	//copies
                    m=strlen(line);// gets the 
            
                    printf("\r                                                                                  ");
                    printf("\rmyshell: %s",line);	//uploads the new line into terminal
                }
                m--;

                break;
            case 'B':	//if down arrow is pressed
                if(currentLine->next!=NULL){
                    currentLine=currentLine->next;
                    //line=currentLine->line;

                    int lineCount=strlen(line);	//empties the line
                    for(int k=0;k<lineCount;k++)
                        line[k]='\0';

                    cpStr(currentLine->line,line);	//copies
                    m=strlen(line);
                }
                m--;
                printf("\r                                                                                  ");
                printf("\rmyshell: %s",line);
                break;
            case 8:	//if next char is backspace or delete
            case 127:
                printf("\r                                                                                  ");	//we reset the line in order to overwrite
                line[m]='\0';
                if(m>0)	//if there is anything to delete, we delete it
                    line[--m]='\0';
                m--;
                printf("\r                                                                                  ");
                printf("\rmyshell: %s",line);	//prints the new line
                break;
            case '\n':			//if line is finished and user prompted \n
                line[m]='\0';
                cpStr(line,newLine->line);	//copies line into newLine-> line to make it accessable in next inputs
                line[m]='\n';	//adds \n in order to make it rest of the file can read it
                line[m+1]='\0';	//finished the char array by \0
                break;
            default:		//copies the line
                line[m]=c;
                cpStr(line,newLine->line);	//copies line into newLine->line
                printf("\r                                                                                    ");
                printf("\rmyshell: %s",line);
                break;
        }
//        printf("hi");


    }
    length= strlen(line);
//    printf("\r  --deleted-- ");
    inputBuffer=line;// umarım bu getch filedan alırken sorun çıkarmıyorsa
    //length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

    if( strcmp(args[ct-1], "&")==0 ){
        args[ct-1] = '\0';
    }

    for (i = 0; i <= ct; i++){
        printf("args %d = %s\n",i,args[i]);
    }

} /* end of setup routine */


typedef struct {
    char** path;
    int pathCount;
} PATH;


PATH* splitPath() {
    int maxPath = 20;
    char** pathPointer;
    char* paths = getenv("PATH");
    char* eachPath;

    pathPointer = (char**)malloc(maxPath * sizeof(char*));

    // here paths are splitted by ':' and loaded into pointer
    int i=0;
    while((eachPath=strtok_r(paths, ":", &paths))) {
        pathPointer[i] = eachPath;
        i++;
    }
    // here to create PATH struct and load the variables
    PATH *pathStruct = (PATH *)malloc(sizeof(PATH));
    pathStruct->pathCount = i;
    pathStruct->path = pathPointer;

    return pathStruct;
}


void findPath(char **args, PATH* path_info, char** realPath) {

    int i;
    int size = path_info->pathCount;

    for(i=0; i<size; i++) {
        char temp_path[128];
        strcpy(temp_path, path_info->path[i]);
        strcat(temp_path, "/");
        strcat(temp_path, args[0]);
        temp_path[strlen(temp_path)] = '\0';

        int canWeReach = access(temp_path, X_OK | F_OK ) ;
        // if we can reach then load the path into real path variable
        if(canWeReach == 0) {
            *(realPath) = strdup(temp_path);
        }
    }

}


typedef struct BackgroundProcessQueue{
    pid_t parentPid;
    pid_t childPid;
    int index;
    char *args;
    char *argName;
    struct BackgroundProcessQueue *next;
} BackgroundProcessQueue;


typedef struct node
{
    int indexOfNode;
    pid_t pid;
    char args[20];
    struct node *next;
}Node;


Node *head;
Node *finished;
Node *foreground;


//below function prints head queue
void printQueue(){
    struct node *temp = head;
    printf("Running : \n");
    while(temp != NULL)
    {
        printf("[%d] %s (pid = %d) \n",temp->indexOfNode,temp->args,temp->pid);
        temp = temp->next;
    }

}

//below function prints finished queue
void printQueueFinished(){
    Node *temp = finished;
    printf("Finished : \n");
    while(temp != NULL)
    {
        printf("[%d] %s (pid = %d) \n",temp->indexOfNode,temp->args,temp->pid);
        temp = temp->next;
    }

}

//with given pid and args struct added to head queue
void addQueue(pid_t pid,char args[20]){

    if(head == NULL){
        head = (Node *) malloc(sizeof(struct node));
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        newNode->indexOfNode = 1;
        strcpy( newNode->args , args );
        newNode->next = NULL;

        head = newNode;


    }else{
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        strcpy( newNode->args , args );
        newNode->next = NULL;

        Node *temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        int ind = temp->indexOfNode;
        newNode->indexOfNode = ++ind;

        temp->next = newNode;
        //printf("\n");
        //printf("wqe %d",temp->next);
    }
}


//with given pid struct added to foreground queue
void addQueueForeground(pid_t pid){

    if(foreground == NULL){
        foreground = (Node *) malloc(sizeof(struct node));
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        newNode->indexOfNode = 1;
        newNode->next = NULL;

        foreground = newNode;


    }else{
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        newNode->next = NULL;

        Node *temp = foreground;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        int ind = temp->indexOfNode;
        newNode->indexOfNode = ++ind;

        temp->next = newNode;
    }
}


//with given index, pid and args struct added to finished queue
void addQueueFinished(pid_t pid,int index,char args[20]){

    //Node *headFinished = finished;
    if(finished == NULL){
        finished = (Node *) malloc(sizeof(struct node));
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        newNode->indexOfNode = index;
        strcpy( newNode->args , args );
        newNode->next = NULL;

        finished = newNode;

    }else{
        Node *newNode = (Node *) malloc(sizeof(struct node));
        newNode->pid = pid;
        strcpy( newNode->args , args );
        newNode->next = NULL;

        Node *temp = finished;
        while (temp->next != NULL) {
            temp = temp->next;
        }

        newNode->indexOfNode = index;

        temp->next = newNode;
    }

}



//delete with given pid in head queue
void deleteNode(pid_t pid) {
    struct node* curr = head;
    struct node* prev = NULL;

    if(curr == NULL){
        head=NULL;
    }
    while(curr->pid != pid){
        prev = curr;
        curr = curr->next;
        if(curr==NULL){
            return;
        }
    }
    if(prev==NULL){
        addQueueFinished(curr->pid,curr->indexOfNode,curr->args);
        head = curr->next;
        return;
    }
    addQueueFinished(curr->pid,curr->indexOfNode,curr->args);
    prev->next=curr->next;

}


void printBackgroundQueue(BackgroundProcessQueue  *backgroundProcessQueue){

    int i = 0;
    while(backgroundProcessQueue != NULL){
        printf("bqueue->child pid = %d, bqueue->args = %s \n",backgroundProcessQueue->parentPid,backgroundProcessQueue->args );
        if(backgroundProcessQueue->childPid==0)
            printf("this is an child\n");
        else
            printf("this is an parent\n");

        i++;
        backgroundProcessQueue = backgroundProcessQueue->next;
    }
    printf("SIZE of background process -> %d \n",i);

}



int execForProcess(char *execPath, char **args, int *isBackground){
    pid_t childPid = fork();

    //below is for error check
    if(childPid == -1) {
        fprintf(stderr, "Fail\n");
        return 1;
    }

    if(*isBackground == 0){
        if(childPid == 0) {    // child enters foreground
            checkInputOutputChoice(args);
            execv(execPath,args);
            fprintf(stderr, "Error\n");
            return 1;
        }
        if(childPid){          // parent enters foreground
            //below 2 lines current process marked
            anyForegroundProcess =true;
            currentForegroundPid = childPid;
            //childpid added to foreground process
            addQueueForeground(childPid);
            //here parent waits for child process*/
            waitpid(childPid, NULL, 0);
            anyForegroundProcess =false;
            return childPid;
        }
    }else if(*isBackground != 0){              //BACKGROUND
        //child background process
        if(childPid == 0){
            sleep(20);
            execv(execPath,args);
            fprintf(stderr, "Error\n");
            return 1;
        }
        //parent background process
        if(childPid){

            addQueue(childPid,args[0]);

            return childPid;
        }
    }
    return childPid;
}





void ps_allFunction(){
    Node *temp = head;
    int status;
    //check if background processes dead, if it is then locate them into finished queue
    while(temp != NULL) {
        if (waitpid(temp->pid, &status, WNOHANG) != 0) {
            deleteNode(temp->pid);
        }
        temp = temp->next;
    }
    //after detecting processes dead or alive then prints first running after finished
    printQueue();
    printQueueFinished();
}

//SEARCH
typedef struct SearchPath
{
    char file[255];
} SearchPath;

void ReadTextFile(char * textPath, char *searchText)
{
    FILE* fp = fopen(textPath, "r");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    // if (fp == NULL)
    //exit(EXIT_FAILURE);

    int currentLine = 0;
    while ((read = getline(&line, &len, fp)) != -1) {

        if (strstr(line, searchText) != NULL)
        {
            int offsetValue = 0;

            for(int i = 0; line[i] == ' ' || line[i] == '\t'; i++)
            {
                offsetValue = i+1;
            }

            printf("%d: %s -> %s", currentLine, textPath, line+offsetValue);
        }

        currentLine++;
    }

    fclose(fp);
    //free(fp);

    // if(fp != NULL)


    if (line)
        free(line);
    //exit(EXIT_SUCCESS);


}


void MainSearch(char* currentPath, char* textToSearch, int isRecursive)
{
    SearchPath* files = (SearchPath*) malloc(sizeof(SearchPath) * 255);
    SearchPath* folders = (SearchPath*) malloc(sizeof(SearchPath) * 255);

    int fileIndex = 0;
    int folderIndex = 0;

    DIR *d;

    struct dirent *dir;

    d = opendir(currentPath);

    if (d)

    {

        while ((dir = readdir(d)) != NULL)

        {
            char *name = (dir->d_name);

            int isSearchableFile = 0;
            int areWeReachedTheDot = 0;

            for(int i = 0; name[i] != '\0' ; i++)
            {
                if(areWeReachedTheDot == 1)
                {
                    if(name[i] == 'c' ||name[i] == 'C' || name[i] == 'H' ||name[i] == 'h')
                        isSearchableFile = 1;
                    break;
                }

                if( name[i] == '.')
                    areWeReachedTheDot = 1;
            }

            if(isSearchableFile)
            {
                strcpy(files[fileIndex].file,name);
                fileIndex++;
            }
            else if (!areWeReachedTheDot)
            {
                strcpy(folders[folderIndex].file,name);
                folderIndex++;
            }
        }


        char *path = currentPath;

        for(int i = 0; i < fileIndex; i++)
        {
            char *textPath = (char*) malloc(sizeof(char) * 255);
            textPath[0] = '\0';

            strcpy(textPath, path);
            strcat(textPath, "/");
            strcat(textPath, (files[i]).file);

            ReadTextFile(textPath, textToSearch);
            free(textPath);


        }


        if(isRecursive == 1)
        {
            for(int i = 0; i < folderIndex; i++)
            {
                char * folderPath = (char*) malloc(sizeof(char) * 255);
                folderPath[0] = '\0';

                strcpy(folderPath, path);
                strcat(folderPath, "/");
                strcat(folderPath, (folders[i]).file);
                MainSearch(folderPath, textToSearch, isRecursive);
                free(folderPath);
            }
        }



    }

    free(files);
    free(folders);
    closedir(d);
}
// BOOKMARK

typedef struct BookmarkData
{
    char command[255];
} BookmarkData;

BookmarkData BookmarkList[255];
int CurrentBookmarkIndex = 0;


void setupp(char inputBuffer[], char *args[],int *background){
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = strlen(inputBuffer)+1;


    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

    if( strcmp(args[ct-1], "&")==0 ){
        args[ct-1] = '\0';
    }


} /* end of setup routine */





void BookmarkCommand(char* command,PATH *pathInformation)
{


    if(command[0] == '"' )
    {
        int index = 0;

        for(int i = 1; command[i] != '"'; i++)
        {
            BookmarkList[CurrentBookmarkIndex].command[index] = command[i];
            index++;
        }
        BookmarkList[CurrentBookmarkIndex].command[index] = '\0';
        CurrentBookmarkIndex++;
    }
    else if (command[0] == '-')
    {
        if (command[1] == 'l')
        {
            // list
            for(int i = 0; i < CurrentBookmarkIndex; i++)
            {
                printf("%d \"%s\"\n", i, (BookmarkList[i].command));
            }
        }
        else
        {

            char *numStr= (char*) malloc(sizeof(char) * 255);
            int index = 0;

            for(int i = 3; command[i] != '\0'; i++)
            {
                numStr[index] = command[i];
                index++;
            }
            numStr[index] = '\0';
            int numInt = atoi(numStr);

            if(command[1] == 'd')
            {
                for(int i = numInt; i < CurrentBookmarkIndex - 1; i++)
                {
                    BookmarkList[i] = BookmarkList[i+1];
                }
                CurrentBookmarkIndex--;
            }
            else if(command[1] == 'i')
            {
                int background;
                char* commandPath;
                char *args[MAX_LINE/2 + 1];
                setupp( strcat(BookmarkList[numInt].command ,"\n") ,args,&background);

                commandPath=NULL;
                findPath(args, pathInformation, &commandPath);
                if(commandPath == NULL)
                {
                    perror("Wrong input file name\n");
                }
                execForProcess(commandPath, args, &background);
                printf("a");


            }


            free(numStr);
        }
    }



}



// ^Z
void z_sig_handler(int signum)
{
    //printf("\n");
    Node *temp = foreground;
    int x=0;
    if(anyForegroundProcess){
        x=1;
        kill(currentForegroundPid,SIGKILL);
        printf("%d is killed - ",currentForegroundPid);
    }
    if(temp==NULL){

        printf("there is no foreground process now\n");
    }
    else{

        while (temp != NULL) {
            if(waitpid(temp->pid, NULL, WNOHANG) == 0) {

                kill(temp->pid, SIGKILL);
                printf("%d is killed \n",temp->pid);
            }

            temp = temp->next;
        }
    }


}




int main(void)
{
    char inputBuffer[MAX_LINE];
    int background;
    char *args[MAX_LINE/2 + 1];
    char* commandPath;
    //here we have signal to catch CTRL+Z
    signal(SIGTSTP, z_sig_handler);

    //With splitPath function we split return of path command by :
    PATH * pathInformation = splitPath();

    while (1) {

        background = 0;
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);

        //here we check if input start with myshell: if it starts with myshell: it
        // is deleted for our program,also it work without myshell: prefix
        char c[9] = "myshell:";
        int i;
        for (i = 0; inputBuffer[i]==c[i]; i++){
            if(i==8){
                for(int j=0; args[j]!=NULL ;j++){
                    args[j] = args[j+1];
                }

            }
        }





        //If input is ps_all parent goes into below, fork itself and wait for child
        if( strcmp(args[0],"ps_all")==0 ){

                ps_allFunction();
                continue;

        }else if(strcmp(args[0],"search")==0){
            pid_t childPid = fork();
            //child
            if(childPid==0) {

                if(strcmp(args[1],"-r")==0) {

                    char *InputVal = args[2];
                    char *searchInput = (char *) malloc(sizeof(char) * 255);
                    int strIndexCounter = 0;
                    for(int i = 0; InputVal[i] != '\0'; i++)
                    {
                        if(InputVal[i] != '"')
                        {
                            searchInput[strIndexCounter] = InputVal[i];
                            strIndexCounter++;
                        }
                    }
                    searchInput[strIndexCounter] = '\0';

                    MainSearch(".", searchInput, 1);


                    free(searchInput);

                }
                else {

                    char *InputVal = args[1];
                    char *searchInput = (char *) malloc(sizeof(char) * 255);
                    int strIndexCounter = 0;
                    for(int i = 0; InputVal[i] != '\0'; i++)
                    {
                        if(InputVal[i] != '"')
                        {
                            searchInput[strIndexCounter] = InputVal[i];
                            strIndexCounter++;
                        }
                    }
                    searchInput[strIndexCounter] = '\0';


                    MainSearch(".", searchInput, 0);

                    free(searchInput);

                }
                exit(0);

            }
            //parent
            if(childPid) {
                //below 2 lines current process marked
                anyForegroundProcess =true;
                currentForegroundPid = childPid;
                //childpid added to foreground process
                addQueueForeground(childPid);
                //here parent waits for child process
                waitpid(childPid, NULL, 0);
                anyForegroundProcess =false;
            }

            continue;
        }else if(strcmp(args[0],"bookmark")==0){

                char *bookmarkInput = (char *) malloc(sizeof(char) * 255);
                bookmarkInput[0] = '\0';

                for (int i = 1; args[i] != NULL; i++) {
                    strcat(bookmarkInput, args[i]);
                    strcat(bookmarkInput, " ");
                }


                BookmarkCommand(bookmarkInput,pathInformation);

                free(bookmarkInput);
                continue;

        }
        else if(strcmp(args[0],"exit")==0){
            Node *temp = head;
            int status;
            while(temp != NULL) {
                //below if statement check if background process dead, if it is then
                // delete from head and add into finished queue
                if (waitpid(temp->pid, &status, WNOHANG) != 0) {
                    deleteNode(temp->pid);
                }
                temp = temp->next;
            }



            if(head == NULL)
            {
                //no background process exist then it exits
                printf("Exitting...");
                exit(0);
            }
            else
            {
                //at least one background process exist, ask for user whether kill or not
                int n;
                printf("There are background processes do you want to exit? (press non zero value)");
                scanf("%d",&n);
                if(n){
                    temp = head;
                    while(temp != NULL)
                    {
                        kill(temp->pid,SIGKILL);
                        temp = temp->next;
                    }

                    exit(0);
                }
                continue;
            }

        }


        //here tries to find path according to input if find appropriate then loads
        // into commandPath variable
        commandPath=NULL;
        findPath(args, pathInformation, &commandPath);
        if(commandPath == NULL)
        {
            perror("Wrong input file name\n");
            continue;
        }
        //below process is for fork and exec
        execForProcess(commandPath, args, &background);









        /** the steps are:
        (1) fork a child process using fork()
        (2) the child process will invoke execv()
        (3) if background == 0, the parent will wait,
        otherwise it will invoke the setup() function again. */
    }
}
