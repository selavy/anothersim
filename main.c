#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_STR 256
#define NUMREGS 32
#define MEMSZ 125
#define WORD 8
#define MAXINST 128
#define NOTUSED -1
#define EMPTY -1
#define STALL 1
#define NOSTALL 0

#define MEMLOC( x ) ( (x) / WORD )
#define MADDR( x )  ( (x) * WORD )
#define RADDR( x )  ( (x) - 1 )

#define _DEBUG

/* TYPES */
typedef int32_t RegVal;
typedef int32_t MemVal;

enum inst_t
  {
    DADD,
    SUB,
    LD,
    SD,
    BNEZ
  };

struct inst
{
  enum inst_t itype;
  char label[MAX_STR];
  size_t labelsz;
  int rs;
  int rt;
  int rd;
  char btarget[MAX_STR];
  size_t btargetsz;
  int32_t value;
};

/* GLOBAL VARS */
struct inst * Instructions[MAXINST];
RegVal Registers[NUMREGS];
MemVal Memory[MEMSZ];
int instcount = 0;
int WB = EMPTY;
int MEM3 = EMPTY;
int MEM2 = EMPTY;
int MEM1 = EMPTY;
int EX = EMPTY;
int ID = EMPTY;
int IF2 = EMPTY;
int IF1 = EMPTY;
int inst_counter = 0;
FILE * fout;

int IF1_stall  = NOSTALL;
int IF2_stall  = NOSTALL;
int ID_stall   = NOSTALL;
int EX_stall   = NOSTALL;
int MEM1_stall = NOSTALL;
int MEM2_stall = NOSTALL;
int MEM3_stall = NOSTALL;
int WB_stall   = NOSTALL;

/* FUNCTIONS DECLARATIONS */
void print_regs();
void print_mem();
void print_insts();
int read_regs( FILE * pFile );
int read_mem( FILE * pFile );
int read_insts( FILE * pFile );
int init( FILE * pFile );
int check_stall( int reg );
void if1();
void if2();
void id();
void ex();
void mem1();
void mem2();
void mem3();
void wb();

int main( int argc, char ** argv )
{
  /* variables */
  const char * file_name = "input.txt";
  FILE * pFile;
  fout = stdout;
  /*-----------*/

  /* Open File */
  pFile = fopen( file_name, "r" );
  if( pFile == NULL ) return 1;
  
  if( init( pFile ) ) goto error;

#ifdef _DEBUG
  /*  print_insts(); */
#endif

  do
    {
      /* Pipeline */
      printf("c#%d ", inst_counter + 1 );
      wb();
      mem3();
      mem2();
      mem1();
      ex();
      id();
      if2();
      if1();
      printf("\n");
      if( !MEM3_stall ) WB = MEM3;
      if( !MEM2_stall ) MEM3 = MEM2;
      if( !MEM1_stall ) MEM2 = MEM1;
      if( !EX_stall ) MEM1 = EX;
      if( !ID_stall ) EX = ID;
      if( !IF2_stall ) ID = IF2;
      if( !IF1_stall ) IF2 = IF1;
      
    } while( (IF1 != EMPTY) || (IF2 != EMPTY) || (ID != EMPTY) || (EX != EMPTY) || (MEM1 != EMPTY) || (MEM2 != EMPTY) || (MEM3 != EMPTY) || (WB != EMPTY) );

  /* end Pipeline */

  /* print end status */
  print_regs();
  print_mem();
  goto end;
 error:
  printf("ERROR Will Robinson!\n");
 end:
  fclose( pFile );
  return 0;
}

/******************************************************************************************************/
/*                            FUNCTIONS DEFINITIONS                                                   */
/******************************************************************************************************/

void print_regs()
{
  int i;
  printf("REGISTERS\n");
  for( i = 0; i < NUMREGS; ++i )
    {
      if( Registers[i] != 0 )
	printf("R%d %d\n", i+1, Registers[i] );
    }
}

void print_mem()
{
  int i;
  printf("MEMORY\n");
  for( i = 0; i < MEMSZ; ++i )
    {
      if( Memory[i] != 0 )
	printf("%d %d\n", i * WORD, Memory[i] );
    }
}

void print_insts()
{
  int i;
  struct inst * curr;
  printf("INSTS\n");
  for( i = 0; i < instcount; ++i )
    {
      printf("------------------\n");
      curr = Instructions[i];
      printf("type: %d\n", (*curr).itype );

      if( (*curr).labelsz > 0 )
	printf("label: %s\n", (*curr).label );

      if( (*curr).rs != NOTUSED )
	printf("rs: %d\n", (*curr).rs );
      
      if( (*curr).rt != NOTUSED )
	printf("rt: %d\n", (*curr).rt );
      
      if( (*curr).rd != NOTUSED )
	printf("rd: %d\n", (*curr).rd );

      if( (*curr).btargetsz > 0 )
	printf("branch target: %s\n", (*curr).btarget );
      
      if( (*curr).value != NOTUSED )
	printf("value: %d\n", (*curr).value );

      printf("-------------------\n");
    }
}

int read_regs( FILE * pFile )
{
  char line[MAX_STR+1];
  int reg;
  RegVal value;

  if( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "REGISTERS", 9 ) != 0 )
	return 1;
    }
  else
    return 1;

  /* Read the Registers Section */
  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "MEMORY", 6 ) == 0 )
	break;

      if( sscanf( line, "R%d %d\n", &reg, &value ) != 2 )
	continue;
      
      if( ( reg >= 0 ) && ( reg < NUMREGS ) )
	Registers[reg - 1] = value;
    }
  return 0;
}

int read_mem( FILE * pFile )
{
  int loc;
  char line[MAX_STR+1];
  RegVal value;
  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "CODE", 4 ) == 0 )
	break;

      if( sscanf( line, "%d %d\n", &loc, &value ) != 2 )
	continue;
      
      loc = MEMLOC( loc );
      if( ( loc >= 0 ) && ( loc < MEMSZ ) )
	Memory[loc] = value;
    }

  return 0;
}

int read_insts( FILE * pFile )
{
  char line[MAX_STR+1];
  char * tmpline;
  char label[MAX_STR+1];
  char target[MAX_STR+1];
  int rd;
  int rs;
  int rt;
  int offset;
  RegVal value;
  struct inst * curr;

  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( Instructions[instcount] == NULL )
	{
	  Instructions[instcount] = malloc( sizeof( Instructions[instcount] ) );
	  if( Instructions[instcount] == NULL )
	    return 1;
	}

      curr = Instructions[instcount];

      if( ( tmpline = strstr( line, ":" ) ) != NULL )
	{
	  sscanf( line, "%s: %*s\n", label );
	  do { tmpline++; } while( isspace( *tmpline ) );	 
	  strcpy( line, tmpline );
	  label[ strlen(label) - 1 ] = '\0';
	  strcpy( (*curr).label, label );
	  (*curr).labelsz = strlen( label );
	}
      else
	{
	  strcpy( label, "" );
	  (*curr).label[0] = '\0';
	  (*curr).labelsz = 0;
	}

#ifdef _DEBUG
      /*      printf("label => %s: ", label ); */
#endif

      if( strstr( line, "DADD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, R%d, R%d\n", &rd, &rs, &rt ) != 3 )
	    {
	      if( sscanf( line, "%*s R%d, R%d, #%d\n", &rd, &rs, &value ) != 3 )
		continue;

	      rt = value; /* for the debug print line */
	      (*curr).rt = NOTUSED;
	      (*curr).value = rt;
	    }
	  else
	    {
	      (*curr).rt = rt;
	      (*curr).value = NOTUSED;
	    }

#ifdef _DEBUG
	  /*	  printf("DADD rd = %d, rs = %d, rt = %d\n", rd, rs, rt ); */
#endif

	  (*curr).itype = DADD;
	  (*curr).rs = rs;
	  (*curr).rd = rd;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	}
      else if( strstr( line, "SUB" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, R%d, R%d\n", &rd, &rs, &rt ) != 3 )
	    {
	      if( sscanf( line, "%*s R%d, R%d, #%d\n", &rd, &rs, &rt ) != 3 )
		continue;

	      rt = value; /* for the debug print line */
	      (*curr).rt = NOTUSED;
	      (*curr).value = rt;
	    }
	  else
	    {
	      (*curr).rt = rt;
	      (*curr).value = NOTUSED;
	    }
	  
#ifdef _DEBUG
	  /* printf("SUB rd = %d, rs = %d, rt = %d\n", rd, rs, rt ); */
#endif

	  (*curr).itype = SUB;
	  (*curr).rs = rs;
	  (*curr).rd = rd;
	  (*curr).btarget[0] = '\0';
	}
      else if( strstr( line, "LD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %d(R%d)\n", &rt, &offset, &rs ) != 3 ) continue;

#ifdef _DEBUG
	  /*	  printf("LD rt = %d, offset = %d, rs = %d\n", rt, offset, rs ); */
#endif

	  (*curr).itype = LD;
	  (*curr).rs = rs;
	  (*curr).rt = rt;
	  (*curr).rd = NOTUSED;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	  (*curr).value = offset;
	}
      else if( strstr( line, "SD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %d(R%d)\n", &rt, &offset, &rs ) != 3 ) continue;

#ifdef _DEBUG
	  /*	  printf("SD rt = %d, offset = %d, rs = %d\n", rt, offset, rs ); */
#endif
	  
	  (*curr).itype = SD;
	  (*curr).rs = rs;
	  (*curr).rt = rt;
	  (*curr).rd = NOTUSED;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	  (*curr).value = offset;
	}
      else if( strstr( line, "BNEZ" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %s\n", &rs, target ) != 2 ) continue;

#ifdef _DEBUG
	  /*	  printf("BNEZ rs = %d, target = %s\n", rs, target ); */
#endif

	  (*curr).itype = BNEZ;
	  (*curr).rs = rs;
	  (*curr).rt = NOTUSED;
	  (*curr).rd = NOTUSED;
	  strcpy( (*curr).btarget, target );
	  (*curr).btargetsz = strlen( target );
	  (*curr).value = NOTUSED;
	}
      else
	{
#ifdef _DEBUG
	  /*  printf("command not recognized: %s\n", line ); */
#endif
	  continue;
	}

	instcount++;
    }

  return 0;
}

int init( FILE * pFile )
{
  int i;

 /* init the registers and memory file */
  for( i = 0; i < NUMREGS; ++i )
    Registers[i] = 0;
  for( i = 0; i < MEMSZ; ++i )
    Memory[i] = 0;
  for( i = 0; i < MAXINST; ++i )
    Instructions[i] = NULL;

  if( read_regs( pFile ) ) return 1;
  if( read_mem( pFile ) ) return 1;
  if( read_insts( pFile ) ) return 1;

  return 0;
}

int check_stall( int reg )
{
  /* Check MEM1 */
  if( EX != EMPTY )
    {
      if( (*Instructions[ MEM1 ]).rd == reg ) return 1;
      if( ( (*Instructions[ MEM1 ]).itype == LD ) && ( (*Instructions[ EX ]).rt == reg  ) ) return 1;
    }
  /* Check MEM2 */
  if( MEM1 != EMPTY )
    {
      if( (*Instructions[ MEM2 ]).rd == reg ) return 1;
      if( ( (*Instructions[ MEM2 ]).itype == LD ) && ( (*Instructions[ MEM2 ]).rt == reg  ) ) return 1;
    }

  return 0;
}

void if1()
{
  if( IF1_stall && (IF1 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", IF1 + 1);
      return;
    }
  if( inst_counter < instcount )
    IF1 = inst_counter;
  else
    IF1 = EMPTY;
  if( IF1 != EMPTY )
    fprintf( fout, "I%d-IF1 ", IF1 + 1 );
  /*  IF2 = IF1; */
  inst_counter++;
}

void if2()
{
  if( IF2_stall && (IF2 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", IF2 + 1);
      return;
    }
  /*  ID = IF2; */
  if( IF2 != EMPTY )
    fprintf( fout, "I%d-IF2 ", IF2 + 1 );
}

void id()
{
  if( ID_stall && (ID != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", ID + 1);
      return;
    }
  /*  EX = ID; */
  if( ID != EMPTY )
    fprintf( fout, "I%d-ID ", ID + 1 );
}

void ex()
{
  struct inst * curr;
  curr = Instructions[EX];

  if( EX_stall && (EX != EMPTY) )
    {
      if( check_stall( (*curr).rs ) || check_stall( (*curr).rt ) )
	{
	  fprintf( fout, "I%d-stall ", EX + 1);
	  return;
	}
      else
	{
	  IF1_stall = NOSTALL;
	  IF2_stall = NOSTALL;
	  ID_stall = NOSTALL;
	  EX_stall = NOSTALL;
	}
    }

  /*  MEM1 = EX; */
  if( EX == EMPTY )
    return;


  
  if( (*curr).itype == DADD )
    {
      if( check_stall( (*curr).rs ) || check_stall( (*curr).rt ) )
	{
	  IF1_stall = STALL;
	  IF2_stall = STALL;
	  ID_stall = STALL;
	  EX_stall = STALL;
	  fprintf( fout, "I%d-stall ", EX + 1 );
	  return;
	}
      if( (*curr).rt == NOTUSED )
	Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] + (*curr).value;
      else
	{
	  /*
	  printf("\n\nDADD:\n");
	  printf("rs = %d, $s = %d\n", (*curr).rs, Registers[ RADDR((*curr).rs) ] );
	  printf("rt = %d, $t = %d\n", (*curr).rt, Registers[ RADDR((*curr).rt) ] );
	  print_regs();
	  print_mem();
	  */
	  Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] + Registers[ RADDR((*curr).rt) ];
	  /*
	  print_regs();
	  print_mem();
	  printf("\n\n");
	  */
	}
    }
  if( (*curr).itype == SUB )
    {
      Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] - Registers[ RADDR((*curr).rt) ];
    }

  fprintf( fout, "I%d-EX ", EX + 1 );
}

void mem1()
{
  if( MEM1_stall && (MEM1 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", MEM1 + 1 );
      return;
    }
  /*  MEM2 = MEM1; */
  if( MEM1 != EMPTY )
    fprintf( fout, "I%d-MEM1 ", MEM1 + 1 );
}

void mem2()
{
  struct inst * curr;
  if( MEM2_stall && (MEM2 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", MEM2 + 1 );
      return;
    }
  /*  MEM3 = MEM2; */
  if( MEM2 == EMPTY )
    return;

  curr = Instructions[MEM2];
  /* TODO : currently not memory safe at all */
  if( (*curr).itype == LD )
    {
      /*
      printf("\n\nLD:\n");
      printf("rs = %d, $s = %d, offset = %d\n", (*curr).rs, Registers[ RADDR((*curr).rs) ], (*curr).value );
      printf("rt = %d, $t = %d\n", (*curr).rt, Registers[ RADDR((*curr).rt) ] );
      print_regs();
      print_mem();
      */
      Registers[ RADDR((*curr).rt) ] = Memory[ MEMLOC( Registers[ RADDR((*curr).rs) ] + (*curr).value) ];
      /*
      printf("\n");
      print_regs();
      print_mem();
      printf("\n\n");
      */
    }
  else if( (*curr).itype == SD ) 
    {
      /*
      printf("\n\nSD:\n");
      printf("rs = %d, $s = %d, offset = %d\n", (*curr).rs, Registers[ RADDR((*curr).rs) ], (*curr).value );
      printf("rt = %d, $t = %d\n", (*curr).rt, Registers[ RADDR((*curr).rt) ] );
      print_regs();
      print_mem();
      */
      Memory[ MEMLOC(Registers[ RADDR((*curr).rs) ] + (*curr).value) ] = Registers[ RADDR((*curr).rt) ];
      /*
      printf("\n");
      print_regs();
      print_mem();
      printf("\n\n");
      */
    }
  fprintf( fout, "I%d-MEM2 ", MEM2 + 1 );
}

void mem3()
{
  if( MEM3_stall && (MEM3 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", MEM3 + 1 );
      return;
    }
  /*  WB = MEM3; */
  if( MEM3 != EMPTY )
    fprintf( fout, "I%d-MEM3 ", MEM3 + 1 );
}

void wb()
{
  if( WB_stall && (WB != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", WB + 1 );
      return;
    }
  if( WB != EMPTY )
       fprintf( fout, "I%d-WB ", WB + 1 );
}
