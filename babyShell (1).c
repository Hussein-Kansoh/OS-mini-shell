#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>


//implementation of getcmd function
int getcmd(char *prompt, char *args[], int *background)
{
  int length = 0; //will store the number of characters in the string
  int i = 0; //start at begining
  char *token; //will be used when we split the input string 
  char *loc; //used to check background 
  size_t linecap = 64; //used as argument in getLine
  char *line = (char *) malloc(sizeof(char)*linecap); //dynamically allocating memory for input string
  printf("%s", prompt); //will be shown before the command output
  length = getline(&line, &linecap, stdin); //puts the string passed in the command line into line and the number of characters into length
  if (length <= 0) {
    exit(-1);  //invalid input
 }

 // Check if background is specified..
   if ((loc = index(line, '&')) != NULL) { //the ampersand exists
     *background = 1;  //set the background to be 1 indicating that parent should not wait 
     *loc = ' ';    //reset loc
   } else
 *background = 0; //parent should wait
   
   while ((token = strsep(&line, " \t\n")) != NULL) { //loop through each "word" in the input
     for (int j = 0; j < strlen(token); j++) //loop through the characters of the word
        if (token[j] <= 32) 
         token[j] = '\0'; //if the token is not a valid character set it to null so it's not put in the array
     if (strlen(token) > 0) 
       args[i++] = token; //fill up args array with the command line input words 
   }
   free(line);
   return i; //the number of words in the command line input
}

int n = 0; //size of array of background pids

//signal handler for SIGSTP
void handle_sigstp(int Signal) {
  // Reset handler
    signal(SIGTSTP, handle_sigstp);
}

//kill handler
void handle_child(int signal) {
  kill(getpid(), SIGTSTP); 
}

//helper method to know if the command is built in or not
int isBuiltIn(char* inputCommand, char* commandName1, char* commandName2, char* commandName3, char* commandName4, char* commandName5){
    return !strcmp(inputCommand,commandName1) || !strcmp(inputCommand,commandName2) || 
    !strcmp(inputCommand,commandName3) || !strcmp(inputCommand,commandName4) || !strcmp(inputCommand,commandName5);
}

//helper method to remove an element from array used to remove element from array of background pids
void removeElement(int startIndex, int size, int inputArray[]){
  for (int i = startIndex - 1; i < size - 1; i++){
          inputArray[i] = inputArray[i+1];
    }
    //decrement array size 
    n--;
}

//helper method to remove processes which have already executed from array of background pids
void removeZombieProcesses(int inputArray[], int length){
  int status;
  for (int i = 0; i<length; i++){
      if(waitpid(inputArray[i], &status, WNOHANG)!=0){
          //remove element from array since process is over
          removeElement(i,length,inputArray);
      }
  }
}

//helper method to get particular index of certain character (used to identify index of pipe or redirection)
int characterIndex(char* inputString, char* inputAarray[], int length){
    for (int i=0; i<length; i++){
      if(strcmp(inputAarray[i], inputString)==0)
      {
        return i;
      }
    } 
    return INT_MAX;
}

//helper method to identify if string is in array of strings or not (used to identify if pipe or redirection is present)
int stringExists(char* inputString, char* inputAarray[], int length){
    for (int i=0; i<length; i++){
      if(!strcmp(inputAarray[i], inputString))
      {
        return 1;
      }
    }
    return 0;
}

int main(void)
{
  char *args[20]; //initilization of args array
  int IDS[20]; //initilization of array that will contain the pid's of background processes
  int bg; //will indicate background
  signal(SIGTSTP, handle_sigstp); //pass SIGSTP to handle_sigint so that program does not suspend when Ctrl+Z is pressed
 
  while(1) {
    bg = 0; //flag indicating of ampersand (background) is present or not
    int cnt = getcmd("\n>> ", args, &bg); //performs getcmd which are now put in args and cnt is number of words in the command line input
    if (cnt == 0){
      continue; //make shell functional even if user keeps pressing enter
    } 
    char* command = args[0]; //store the first string (which is the actual command)
    args[cnt]=NULL; //make last element null so execVp is able to run
    char* cd = "cd";
    char* pwd = "pwd";
    char* Exit = "exit";
    char* fg = "fg";
    char* jobs = "jobs"; //strings of built in commands
    if(isBuiltIn(command, cd, pwd, Exit, fg, jobs)){ //check if command is built in
    if (strcmp(command, cd) == 0){ 
      chdir(args[1]);   //execute cd
    } else if (strcmp(command, pwd) == 0){
      size_t size = 64; 
      char *buf = (char *) malloc(sizeof(char)*size); 
      printf("%s\n", getcwd(buf, size)); //execute pwd
      free(buf);
    }
      else if (strcmp(command, Exit) == 0){
        kill(0, SIGSTOP); //execute exit
    }
      else if (strcmp(command, fg) == 0){
        removeZombieProcesses(IDS, n); //remove zombie processes before checking on index
        int index = atoi(args[1]);  //check index given by user
        waitpid(IDS[index-1], NULL, 0); //call wait on that process to have it pushed to the foreground
        removeElement(index, n, IDS);  //remove element once its been pushed to the foreground
    } else if (strcmp(command, jobs) == 0){
       removeZombieProcesses(IDS, n); //remove zombie processes before printing out background jobs
      for (int i = 0; i < n; i++){ 
        printf("ID of job %d is: %d\n", i+1, IDS[i]); //print out jobs
      }
    }
    }else{
    int fd[2]; //2 way pipe
    if (pipe(fd)==-1){
      fprintf(stderr, "error with pipe"); //check pipe is working
    }
    int pid = fork(); //fork the main process
    if (stringExists("|", args, cnt)){  //check if user wants to pipe
      int pipeIndex = characterIndex("|", args, cnt); //find index where pipe occurs
      char* tempArray2[10]; //array which will contain command up to the '|'
      if (pid==0){ //inside child
        dup2(fd[1], STDOUT_FILENO); //make standard output the writing end of the pipe
        for (int i=0; i<pipeIndex; i++){
            tempArray2[i]=args[i]; //copy command up to the '|'
        }
        tempArray2[pipeIndex]=NULL; //make last element null to be able to execute execvp
        close(fd[0]); 
        close(fd[1]); //close
        execvp(command, tempArray2); //execute command up to the '|' with new standard output
      }      
      int pid2 = fork(); //fork again to create new process for standard input
      if (pid2<0){
        fprintf(stderr, "fork failed");  //Failed to fork
      }  
      if (pid2==0){ //in 2nd process child
        char* tempArray3[10]; //array which will contain command after the '|'
        dup2(fd[0], STDIN_FILENO); //make standard input the reading end of the pipe
        int j = 0;
        for (int i=pipeIndex+1; i<cnt; i++){
          tempArray3[j]=args[i]; //copy command after the '|'
          j++;
        }
        close(fd[0]);
        close(fd[1]);
        args[cnt]=NULL; //make last element null to be able to execute execvp
        execvp(args[pipeIndex+1], tempArray3); //execute command after the '|' with new standard input
      }
      close(fd[0]);
      close(fd[1]);
      waitpid(pid, NULL, 0);
      waitpid(pid2, NULL, 0); //wait for both children
    }
   if (pid == 0){ //in child process
   //sleep(10);
   if (bg == 1){
     //sleep(15);
   }
    if (stringExists("|", args, cnt) == 0){ //check there is no pipe
      if (args[1] == NULL){
          execvp(command, args); //if only 1 string in args then execute the command (impossible redirection occurs)
      }
      else if (stringExists(">", args, cnt)){ //check if user wants to redirect
        int redirIndex = characterIndex(">",args,cnt); //find index where redirection string is at 
        int file = open(args[redirIndex+1], O_WRONLY | O_CREAT , 0777); //open file with write and create which is named as chosen by the user
        if (file < 0){
          fprintf(stderr, "error with file"); //failed to create family
        }
        char* tempArray[10]; //array which will contain command up to the '>'
        dup2(file, STDOUT_FILENO); //make standard output go to the created file
        for (int i=0; i<redirIndex; i++){
            tempArray[i]=args[i]; //copy command up the '>'
        }
        execvp(command, tempArray); //execute command which's output will now be written to the created file
        close(file); //close the file
      } 
      else{
        execvp(command, args); //if '>' not wanted by user execute command normally
      }
    }
    }
    else if (pid<0){  
      fprintf(stderr, "fork failed");  //Failed to fork
    }
    else {  //in parent process
      if (bg == 0){ //not a bakcground process
         signal(SIGINT, handle_child);  //signal for ctrl-c
         waitpid(pid, NULL, 0); //its not a background so wait
      } else{ //is a background process
          IDS[n] = pid; //add the new background process' pid to array storing background process pids
          n++; //increment the size of the array
      }
    }
  }
}
}