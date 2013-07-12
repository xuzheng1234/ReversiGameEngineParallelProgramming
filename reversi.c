

#include <cilk/cilk.h>

#include <stdio.h>
#include <stdlib.h>

#define BIT 0x1

#define X_BLACK 0
#define O_WHITE 1
#define OTHERCOLOR(c) (1-(c))

/* 
  represent game board squares as a 64-bit unsigned integer.
	these macros index from a row,column position on the board
	to a position and bit in a game board bitvector
*/
#define BOARD_BIT_INDEX(row,col) ((8 - (row)) * 8 + (8 - (col)))
#define BOARD_BIT(row,col) (0x1LL << BOARD_BIT_INDEX(row,col))
#define MOVE_TO_BOARD_BIT(m) BOARD_BIT(m.row, m.col)

/* all of the bits in the row 8 */
#define ROW8 \
  (BOARD_BIT(8,1) | BOARD_BIT(8,2) | BOARD_BIT(8,3) | BOARD_BIT(8,4) |	\
   BOARD_BIT(8,5) | BOARD_BIT(8,6) | BOARD_BIT(8,7) | BOARD_BIT(8,8))
			  
/* all of the bits in column 8 */
#define COL8 \
  (BOARD_BIT(1,8) | BOARD_BIT(2,8) | BOARD_BIT(3,8) | BOARD_BIT(4,8) |	\
   BOARD_BIT(5,8) | BOARD_BIT(6,8) | BOARD_BIT(7,8) | BOARD_BIT(8,8))

/* all of the bits in column 1 */
#define COL1 (COL8 << 7)

#define IS_MOVE_OFF_BOARD(m) (m.row < 1 || m.row > 8 || m.col < 1 || m.col > 8)
#define IS_DIAGONAL_MOVE(m) (m.row != 0 && m.col != 0)
#define MOVE_OFFSET_TO_BIT_OFFSET(m) (m.row * 8 + m.col)

typedef unsigned long long ull;

/* 
	game board represented as a pair of bit vectors: 
	- one for x_black disks on the board
	- one for o_white disks on the board
*/
typedef struct { ull disks[2]; } Board;

typedef struct { int row; int col; } Move;

typedef struct 
{
	Move m;
	int value;
	//int length;
	struct Node * next;
} Node;


Board start = { 
	BOARD_BIT(4,5) | BOARD_BIT(5,4) /* X_BLACK */, 
	BOARD_BIT(4,4) | BOARD_BIT(5,5) /* O_WHITE */
};
 
Move offsets[] = {
  {0,1}		/* right */,		{0,-1}		/* left */, 
  {-1,0}	/* up */,		{1,0}		/* down */, 
  {-1,-1}	/* up-left */,		{-1,1}		/* up-right */, 
  {1,1}		/* down-right */,	{1,-1}		/* down-left */
};

int noffsets = sizeof(offsets)/sizeof(Move);
char diskcolor[] = { '.', 'X', 'O', 'I' };


void PrintDisk(int x_black, int o_white)
{
//printf(" %d",x_black+ (o_white<<1));
  printf(" %c", diskcolor[x_black + (o_white << 1)]);
}

void PrintBoardRow(int x_black, int o_white, int disks)
{
  if (disks > 1) {
    PrintBoardRow(x_black >> 1, o_white >> 1, disks - 1);
  }
  PrintDisk(x_black & BIT, o_white & BIT);
}

void PrintBoardRows(ull x_black, ull o_white, int rowsleft)
{

  if (rowsleft > 1) {
    PrintBoardRows(x_black >> 8, o_white >> 8, rowsleft - 1);
  }
  printf("%d", rowsleft);
 
  PrintBoardRow((int)(x_black & ROW8),  (int) (o_white & ROW8), 8);
  printf("\n");
}

void PrintBoard(Board b)
{
  printf("  1 2 3 4 5 6 7 8\n");
  PrintBoardRows(b.disks[X_BLACK], b.disks[O_WHITE], 8);
}

/* 
	place a disk of color at the position specified by m.row and m,col,
	flipping the opponents disk there (if any) 
*/
void PlaceOrFlip(Move m, Board *b, int color) 
{
  ull bit = MOVE_TO_BOARD_BIT(m);
  b->disks[color] |= bit;
  b->disks[OTHERCOLOR(color)] &= ~bit;
}

/* 
	try to flip disks along a direction specified by a move offset.
	the return code is 0 if no flips were done.
	the return value is 1 + the number of flips otherwise.
*/
int TryFlips(Move m, Move offset, Board *b, int color, int verbose, int domove)
{
  Move next;
  next.row = m.row + offset.row;
  next.col = m.col + offset.col;

  if (!IS_MOVE_OFF_BOARD(next)) {
    ull nextbit = MOVE_TO_BOARD_BIT(next);
    if (nextbit & b->disks[OTHERCOLOR(color)]) {
      int nflips = TryFlips(next, offset, b, color, verbose, domove);
      if (nflips) {
	if (verbose) printf("flipping disk at %d,%d\n", next.row, next.col);
	if (domove) PlaceOrFlip(next,b,color);
	return nflips + 1;
      }
    } else if (nextbit & b->disks[color]) return 1;
  }
  return 0;
} 

int FlipDisks(Move m, Board *b, int color, int verbose, int domove)
{
  int i;
  int nflips = 0;
	
  /* try flipping disks along each of the 8 directions */
  for(i=0;i<noffsets;i++) {
    int flipresult = TryFlips(m,offsets[i], b, color, verbose, domove);
    nflips += (flipresult > 0) ? flipresult - 1 : 0;
  }
  return nflips;
}

void ReadMove(int color, Board *b)
{
  Move m;
  ull movebit;
  for(;;) {
    printf("Enter %c's move as 'row,col': ", diskcolor[color+1]);
    scanf("%d,%d",&m.row,&m.col);
		
    /* if move is not on the board, move again */
    if (IS_MOVE_OFF_BOARD(m)) {
      printf("Illegal move: row and column must both be between 1 and 8\n");
      PrintBoard(*b);
      continue;
    }
    movebit = MOVE_TO_BOARD_BIT(m);
		
    /* if board position occupied, move again */
    if (movebit & (b->disks[X_BLACK] | b->disks[O_WHITE])) {
      printf("Illegal move: board position already occupied.\n");
      PrintBoard(*b);
      continue;
    }
		
    /* if no disks have been flipped */ 
    {
		
      int nflips = FlipDisks(m, b,color, 1, 1);
      if (nflips == 0) {
	printf("Illegal move: no disks flipped\n");
	PrintBoard(*b);
	continue;
      }
	  
      PlaceOrFlip(m, b, color);
   //   printf("You flipped %d disks\n", nflips);
      PrintBoard(*b);
    }
    break;
  }
}

/*
	return the set of board positions adjacent to an opponent's
	disk that are empty. these represent a candidate set of 
	positions for a move by color.
*/
Board NeighborMoves(Board b, int color)
{
  int i;
  Board neighbors = {0,0};
  for (i = 0;i < noffsets; i++) {
    ull colmask = (offsets[i].col != 0) ? 
      ((offsets[i].col > 0) ? COL1 : COL8) : 0;
    int offset = MOVE_OFFSET_TO_BIT_OFFSET(offsets[i]);

    if (offset > 0) {
      neighbors.disks[color] |= 
	(b.disks[OTHERCOLOR(color)] >> offset) & ~colmask;
    } else {
      neighbors.disks[color] |= 
	(b.disks[OTHERCOLOR(color)] << -offset) & ~colmask;
    }
  }
  neighbors.disks[color] &= ~(b.disks[X_BLACK] | b.disks[O_WHITE]);
  return neighbors;
}

/*
	return the set of board positions that represent legal
	moves for color. this is the set of empty board positions  
	that are adjacent to an opponent's disk where placing a
	disk of color will cause one or more of the opponent's
	disks to be flipped.
*/
int EnumerateLegalMoves(Board b, int color, Board *legal_moves)
{
  static Board no_legal_moves = {0,0};
  Board neighbors = NeighborMoves(b, color);
  ull my_neighbor_moves = neighbors.disks[color];
  int row;
  int col;
	
  int num_moves = 0;
  *legal_moves = no_legal_moves;
	
  for(row=8; row >=1; row--) {
    ull thisrow = my_neighbor_moves & ROW8;
    for(col=8; thisrow && (col >= 1); col--) {
      if (thisrow & COL8) {
	Move m = { row, col };
	if (FlipDisks(m, &b, color, 0, 0) > 0) {
	  legal_moves->disks[color] |= BOARD_BIT(row,col);
	  num_moves++;
	}
      }
      thisrow >>= 1;
    }
    my_neighbor_moves >>= 8;
  }
  return num_moves;
}

Node *findMove(Board b, int color)
{
	 Node *head=NULL;
	  Node *current;
  Board neighbors = NeighborMoves(b, color);
  ull my_neighbor_moves = neighbors.disks[color];
  int row;
  int col;
	
 //  int count=0;

	
  for(row=8; row >=1; row--) {
    ull thisrow = my_neighbor_moves & ROW8;
    for(col=8; thisrow && (col >= 1); col--) {
      if (thisrow & COL8) {
	Move m = { row, col };

	
	
	if (FlipDisks(m, &b, color, 0, 0) > 0) {

			 current=(Node*)malloc(sizeof(Node));
			current->m=m;
			//current->value=0;
			current->next=head;
			head=current;
			//count++;
			  
	
	}
      }
      thisrow >>= 1;
    }
    my_neighbor_moves >>= 8;
  }
 // head->length=count;
  return head;

}
Node * Min(Board *b, int color,int cutoff, Node * head,int * alpha,int * beta);
Node * Max(Board *b, int color,int cutoff, Node * head,int * alpha,int * beta);


Node * Max(Board *b, int color,int cutoff, Node * head,int * alpha,int * beta)
{

	Node *list=findMove(*b,color);
	
	

	if(list==NULL || cutoff==0)
	{
		
		 int myscore = CountBitsOnBoard(&b,color);
  		int yourscore = CountBitsOnBoard(&b,1-color);
		//if(head==NULL)
		//head=(Node*)malloc(sizeof(Node));		
		if(head!=NULL)
		head->value=myscore-yourscore;
		
	
		
		return head;
	}
	else
	{
		//Node * result=(Node*)malloc(list->length *sizeof(Node));
		Node * start=list;
		cutoff--;
		Board *firstB=(Board*)malloc(sizeof(Board));
				
				firstB->disks[0]=b->disks[0];
				firstB->disks[1]=b->disks[1];
				
				FlipDisks(list->m, firstB,color, 0, 1);
  				
     		 		PlaceOrFlip(list->m, firstB, color);
		Min(firstB,1-color,cutoff,list,alpha,beta);
		alpha=list->value;
		list=list->next;
 		 while(list!=NULL)
 		 {
				Board *newB=(Board*)malloc(sizeof(Board));
				
				newB->disks[0]=b->disks[0];
				newB->disks[1]=b->disks[1];
				
				FlipDisks(list->m, newB,color, 0, 1);
  				
     		 		PlaceOrFlip(list->m, newB, color);
				
	 			cilk_spawn Min(newB,1-color,cutoff,list,alpha,beta);
				
					
				
				if(list->value!=NULL)
				{
					if(list->value>=beta)
					{
							 
							if(head!=NULL)
							{
								head->value=list->value;
							}
							else
							{
								head=(Node*)malloc(sizeof(Node));
								head->value=list->value;
								head->m=list->m;
							}
							return head;
							
					}
					else
					{
						if(list->value>alpha)
						{
							alpha=list->value;
						
						}
					
					}				
				}
	 			 list=list->next;
  		}
		
		cilk_sync;
		Node * max=start;
		
		start=start->next;
		while(start!=NULL)
		{
			//printf("Move %d %d Value %d\n",(start->m).row, (start->m).col,start->value);
			if(start->value > max->value)
			{
				max=start;
			}
			start=start->next;

		}
		if(head==NULL)
		{
			head=(Node*)malloc(sizeof(Node));
			head->value=max->value;
			head->m=max->m;
		}
		else
		{
			head->value=max->value;
		}


		
 		return head;

 		
	}

}
Node * Min(Board *b, int color,int cutoff, Node * head,int * alpha,int * beta)
{

	Node *list=findMove(*b,color);
	
	

	if(list==NULL || cutoff==0)
	{
		
		 int myscore = CountBitsOnBoard(&b,color);
  		int yourscore = CountBitsOnBoard(&b,1-color);
		//if(head==NULL)
		//head=(Node*)malloc(sizeof(Node));		
		if(head!=NULL)
		head->value=yourscore-myscore;
		
		
		
		return head;
	}
	else
	{
		Node * start=list;
		cutoff--;
		Board *firstB=(Board*)malloc(sizeof(Board));
				
				firstB->disks[0]=b->disks[0];
				firstB->disks[1]=b->disks[1];
				
				FlipDisks(list->m, firstB,color, 0, 1);
  				
     		 		PlaceOrFlip(list->m, firstB, color);
		Max(firstB,1-color,cutoff,list,alpha,beta);
		beta=list->value;
		list=list->next;
		
 		 while(list!=NULL)
 		 {
				Board *newB=(Board*)malloc(sizeof(Board));
				
				newB->disks[0]=b->disks[0];
				newB->disks[1]=b->disks[1];
				
				FlipDisks(list->m, newB,color, 0, 1);
  				
     		 		PlaceOrFlip(list->m, newB, color);
				
	 			cilk_spawn Max(newB,1-color,cutoff,list,alpha,beta);
				
				if(list->value!=NULL)
				{
					if(list->value<=alpha)
					{
							 
							if(head!=NULL)
							{
								head->value=list->value;
							}
							else
							{
								head=(Node*)malloc(sizeof(Node));
								head->value=list->value;
								head->m=list->m;
							}
							return head;
							
					}
					else
					{
						if(list->value<beta)
						{
							beta=list->value;
						
						}
					
					}				
				}
	 			 list=list->next;
  		}
		
		cilk_sync;
		Node * min=start;
		
		start=start->next;
		while(start!=NULL)
		{
			if(start->value!=NULL)
			{
				if(start->value < min->value)
				{
					min=start;
				}
			}
			start=start->next;

		}
		if(head==NULL)
		{
			head=(Node*)malloc(sizeof(Node));
			head->value=min->value;
			head->m=min->m;
		}
		else
		{
			head->value=min->value;
		}


		
 		return head;
	}

}
int ComputerTurn(Board *b, int color,int cutoff)
{
	
  //Node *head=findMove(*b,color);
 
/*
  while(head!=NULL)

  {
	  Move m=head->m;
	  printf("%d %d\n",m.row,m.col);

	  head=head->next;
  }



  printf("\n\n");

  Board legal_moves;

  int num_moves = EnumerateLegalMoves(*b, color, &legal_moves);

    PrintBoard(legal_moves);
*/
	int alpha=-100;
	int beta=100;
	Node * result=Max(b, color,cutoff,NULL,&alpha,&beta);
	if(result!=NULL)
	{
        	Move m=result->m;
	
		printf("Player %d make move %d %d\n",color,m.row,m.col);
		int nflips = FlipDisks(m, b,color, 0, 1);
  
     		 PlaceOrFlip(m, b, color);
      		//printf("You flipped %d disks\n", nflips);
      		PrintBoard(*b);	

		return 1;
	}
	
	
	return 0;
}
int HumanTurn(Board *b, int color)
{
  Board legal_moves;
  int num_moves = EnumerateLegalMoves(*b, color, &legal_moves);
  if (num_moves > 0) {
    ReadMove(color, b);
    return 1;
  } else return 0;
}

int CountBitsOnBoard(Board *b, int color)
{
  ull bits = b->disks[color];
  int ndisks = 0;
  for (; bits ; ndisks++) {
    bits &= bits - 1; // clear the least significant bit set
  }
  return ndisks;
}

void EndGame(Board b)
{
  int o_score = CountBitsOnBoard(&b,O_WHITE);
  int x_score = CountBitsOnBoard(&b,X_BLACK);
  printf("Game over. \n");
  if (o_score == x_score)  {
    printf("Tie game. Each player has %d disks\n", o_score);
  } else { 
    printf("X has %d disks. O has %d disks. %c wins.\n", x_score, o_score, 
	      (x_score > o_score ? 'X' : 'O'));
  }
}

int main (int argc, const char * argv[]) 
{

if (argc != 3) {
	printf("Please enter cutoff for CPU\n");
	exit(-1);
    }

int cut1,cut2;
cut1=atoi(argv[1]);
cut2=atoi(argv[2]);
  Board gameboard = start;
  int move_possible;

 
 PrintBoard(gameboard);
 
/*
  do {
    move_possible = 
      HumanTurn(&gameboard, X_BLACK) | 
      HumanTurn(&gameboard, O_WHITE);
  } while(move_possible);
*/
do {
    move_possible = 
      ComputerTurn(&gameboard, X_BLACK,cut1) | 
      ComputerTurn(&gameboard,O_WHITE,cut2);
  } while(move_possible);
	

  EndGame(gameboard);
	
  return 0;
}
