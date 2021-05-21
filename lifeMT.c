#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
pthread_barrier_t bar;
typedef struct BoardTag {
   int row;     // number of rows
   int col;     // number of columns
   char** src;  // 2D matrix holding the booleans (0 = DEAD , 1 = ALIVE). 
} Board;
typedef struct Share {
    int fr;
   int br;//which row is ready to be worked on
  
   Board*out;
   Board* in;
   int work;//work for each thread;
   int rem;//remaining work if not divided evenly
   int iter;
   pthread_mutex_t mtx;//for logic
   
 
  
   
} Share;
/**
 * Allocates memory to hold a board of rxc cells. 
 * The board is made thoroidal (donut shaped) by simply wrapping around
 * with modulo arithmetic when going beyond the end of the board.
 */
Board* makeBoard(int r,int c)
{
   Board* p = (Board*)malloc(sizeof(Board));
   p->row = r;
   p->col = c;
   p->src = (char**)malloc(sizeof(char*)*r);
   int i;
   for(i=0;i<r;i++)
      p->src[i] = (char*)malloc(sizeof(char)*c);
   return p;
}
/**
 * Deallocate the memory used to represent a board
 */
void freeBoard(Board* b)
{
   int i;
   for(i=0;i<b->row;i++)
      free(b->src[i]);
   free(b->src);
   free(b);
}

/**
 * Reads a board from a file named `fname`. The routine allocates
 * a board and fills it with the data from the file. 
 */
Board* readBoard(char* fName)
{
   int row,col,i,j;
   FILE* src = fopen(fName,"r");
   fscanf(src,"%d %d\n",&row,&col);
   Board* rv = makeBoard(row,col);
   for(i=0;i<row;i++) {
      for(j=0;j<col;j++) {
         char ch = fgetc(src);
         rv->src[i][j] = ch == '*';
      }
      char skip = fgetc(src);
      while (skip != '\n') skip = fgetc(src);
   }
   fclose(src);   
   return rv;
}

/**
 * Save a board `b` into a FILE pointed to by `fd`
 */
void saveBoard(Board* b,FILE* fd)
{
   int i,j;
   for(i=0;i<b->row;i++) {
      fprintf(fd,"|");
      for(j=0;j < b->col;j++) 
         fprintf(fd,"%c",b->src[i][j] ? '*' : ' ');
      fprintf(fd,"|\n");
   }
}
/**
 * Low-level convenience API to clear a terminal screen
 */
void clearScreen()
{
   static const char *CLEAR_SCREEN_ANSI = "\e[1;1H\e[2J";
   static int l = 10;
   write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, l);
}

/*
 * For debugging purposes. print the board on the standard
 * output (after clearing the screen)
 */
void printBoard(Board* b)
{
   //clearScreen();	
   saveBoard(b,stdout);
}

/*
 * Simple routine that counts the number of neighbors that
 * are alive around cell (i,j) on board `b`.
 */
int liveNeighbors(int i,int j,Board* b)
{
   const int pc = (j-1) < 0 ? b->col-1 : j - 1;
   const int nc = (j + 1) % b->col;
   const int pr = (i-1) < 0 ? b->row-1 : i - 1;
   const int nr = (i + 1) % b->row;
   int xd[8] = {pc , j , nc,pc, nc, pc , j , nc };
   int yd[8] = {pr , pr, pr,i , i , nr , nr ,nr };
   int ttl = 0;
   int k;
   for(k=0;k < 8;k++)
      ttl += b->src[yd[k]][xd[k]];
   return ttl;
}

/*
 * Sequential routine that writes into the `out` board the 
 * result of evolving the `src` board for one generation.
 */
void evolveBoard(Board* src,Board* out)
{
   static int rule[2][9] = {
      {0,0,0,1,0,0,0,0,0},
      {0,0,1,1,0,0,0,0,0}
   };
   int i,j;
   for(i=0;i<src->row;i++) {
      for(j=0;j<src->col;j++) {
         int ln = liveNeighbors(i,j,src);
         int c  = src->src[i][j];
         out->src[i][j] = rule[c][ln];
      }
   }
}
void evolveMT(int br,int fr,Board* src,Board* out){
    static int rule[2][9] = {
      {0,0,0,1,0,0,0,0,0},
      {0,0,1,1,0,0,0,0,0}
   };
   int i=br;
   for(i;i<fr;i++) {
      for(int j=0;j<src->col;j++) {
         int ln = liveNeighbors(i,j,src);
         int c  = src->src[i][j];
         out->src[i][j] = rule[c][ln];
      }
   }

    
}
void* thread_work(void* share){
    Share* shr=share;
   
    int myGen=0;
    Board *b0;
    Board *b1;
  
    for(myGen;myGen<shr->iter;myGen++){
    
        b0 = myGen & 0x1 ? shr->out : shr->in; 
        b1 = myGen & 0x1 ? shr->in : shr->out; 
        evolveMT( shr->br, shr->fr,b0, b1);
        pthread_barrier_wait(&bar);
        
    }
        
    
    
}

int main(int argc,char* argv[])
{
   if (argc < 3) {
      printf("Usage: lifeMT <dataFile> #iter #workers\n");
      exit(1);
   }
   Board* life1 = readBoard(argv[1]);
   Board* life2 = makeBoard(life1->row,life1->col);
   
   int nbi = atoi(argv[2]);
   int nbw = atoi(argv[3]);
   
  
    int work;//row perworker
    int rem=0;
    if(life1->row%nbw==0){
        work=life1->row/nbw;
    }
    else{
        rem=life1->row%nbw;
        work=(life1->row-rem)/nbw;
    }
    pthread_t tid[nbw];
    
    Share share[nbw];
    int br=0;
    for(int t=0; t<nbw; t++){
        share[t].iter=nbi;
       
        share[t].br=br;
        br+=work;
        if (rem>0){
            br+=1;
            rem-=1;
            }
        share[t].fr=br;
        share[t].out=life2;
        share[t].in=life1;
      
    
        
    }
  pthread_barrier_init(&bar, NULL,nbw);

    
    for(int i=0; i<nbw;i++)
    {
        pthread_create(tid + i,NULL,thread_work,&share[i]);
        
    }
    for(int k=0;k < nbw;k++){
      pthread_join(tid[k],NULL);
      }
      Board* fb=(nbi%2==0)?life1:life2;
      printBoard(fb);
       FILE* final = fopen("final.txt","w");
       saveBoard(fb,final);
       fclose(final);
       freeBoard(life1);
       freeBoard(life2);
     
   
   /*
    * This is where you should implement your multi-threaded version.
    * Feel free to add as many auxiliary routines (helper functions)
    * As you see fit.
    */

   return 0;
} 
