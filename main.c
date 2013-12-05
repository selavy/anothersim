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
#define TRUE 1
#define FALSE 0

#define MEMLOC( x ) ( (x) / WORD )
#define MADDR( x )  ( (x) * WORD )
#define RADDR( x )  ( (x) - 1 )

/* #define _DEBUG */

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
int WB_count = 0;
int MEM3 = EMPTY;
int MEM3_count = 0;
int MEM2 = EMPTY;
int MEM2_count = 0;
int MEM1 = EMPTY;
int MEM1_count = 0;
int EX = EMPTY;
int EX_count = 0;
int ID = EMPTY;
int ID_count = 0;
int IF2 = EMPTY;
int IF2_count = 0;
int IF1 = EMPTY;
int IF1_count = 1;
int inst_counter = 0;
int flush = FALSE;
int cycle = 1;
int inst_cycle = 1;
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
void print_regs( FILE * pFile );
void print_mem( FILE * pFile );
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
  FILE * pFile;
  int i;
  char in_file[MAX_STR+1];
  char out_file[MAX_STR+1];
  int do_continue;
  char line[MAX_STR+1];

  /* initialize variables */
  do_continue = 0;
  i = 0;
  pFile = NULL;

  /* Here we go... */
do
  {
  printf("Input file? ");
  fgets( in_file, MAX_STR, stdin );

  printf("Output file? ");
  fgets( out_file, MAX_STR, stdin );

  /* chomp the newline char from both */
  sscanf( in_file, "%s\n", in_file );
  sscanf( out_file, "%s\n", out_file );

  /* Open File */
  pFile = fopen( in_file, "r" );
  if( pFile == NULL ) { printf("unable to open input file: %s\n", in_file ); return 1; }

  fout = fopen( out_file, "w" );
  if( fout == NULL ) { printf("unable to open output file: %s\n", out_file ); return 2; }
  
  if( init( pFile ) ) goto unable_to_parse;

#ifdef _DEBUG
  print_insts();
#endif

  do 
    {
      /* Pipeline */
      fprintf( fout, "c#%d ", cycle );
      wb();
      mem3();
      mem2();
      mem1();
      ex();
      id();
      if2();
      if1();
      fprintf( fout, "\n");
      if( !MEM3_stall ) { WB = MEM3; WB_count = MEM3_count; }
      if( !MEM2_stall ) { MEM3 = MEM2; MEM3_count = MEM2_count; }
      if( !MEM1_stall ) { MEM2 = MEM1; MEM2_count = MEM1_count; }
      if( !EX_stall   ) { MEM1 = EX; MEM1_count = EX_count; }
      if( !ID_stall   ) { EX = ID; EX_count = ID_count; } else MEM1 = EMPTY;
      if( !IF2_stall  ) { ID = IF2; ID_count = IF2_count; }
      if( !IF1_stall  ) { IF2 = IF1; IF2_count = IF1_count; ++inst_cycle; }
      if( flush ) { IF1 = EMPTY; IF2 = EMPTY; ID = EMPTY; EX = EMPTY; flush = FALSE; --inst_cycle; }
      ++cycle;
    } while( ( (IF1 != EMPTY) || (IF2 != EMPTY) || (ID != EMPTY) || (EX != EMPTY) || (MEM1 != EMPTY) || (MEM2 != EMPTY) || (MEM3 != EMPTY) || (WB != EMPTY) ) );

  /* end Pipeline */

  /* print end status */
  print_regs( fout );
  print_mem( fout );
  goto ask_to_go_again;

  unable_to_parse:
  fprintf( fout, "Error in input file: %s\n", in_file );
  
  ask_to_go_again:
  for( i = 0; i < instcount; ++i )
    free( Instructions[i] );
  fclose( pFile );
  fclose( fout );

  printf("would you like to run again? ");
  fgets( line, MAX_STR, stdin );
  if( line[0] == 'y' || line[0] == 'Y' )
    do_continue = 1;
  else
    do_continue = 0;

  } while( do_continue );

 goto end;
 
 /* error: */
 fprintf( fout, "Unrecoverable error\n");
 for( i = 0; i < instcount; ++i )
   free( Instructions[i] );
 fclose( pFile );
 fclose( fout );
 end:
 return 0;
}

/******************************************************************************************************/
/*                            FUNCTIONS DEFINITIONS                                                   */
/******************************************************************************************************/

void print_regs( FILE * pFile )
{
  int i;
  fprintf( pFile, "REGISTERS\n" );
  for( i = 0; i < NUMREGS; ++i )
    {
      if( Registers[i] != 0 )
	fprintf( pFile, "R%d %d\n", i+1, Registers[i] );
    }
}

void print_mem( FILE * pFile )
{
  int i;
  fprintf( pFile, "MEMORY\n" );
  for( i = 0; i < MEMSZ; ++i )
    {
      if( Memory[i] != 0 )
	fprintf( pFile, "%d %d\n", i * WORD, Memory[i] );
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
	  Instructions[instcount] = malloc( sizeof( *Instructions[instcount] ) );
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
	  label[0] = '\0';
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

  instcount = 0;
  WB = EMPTY;
  WB_count = 0;
  MEM3 = EMPTY;
  MEM3_count = 0;
  MEM2 = EMPTY;
  MEM2_count = 0;
  MEM1 = EMPTY;
  MEM1_count = 0;
  EX = EMPTY;
  EX_count = 0;
  ID = EMPTY;
  ID_count = 0;
  IF2 = EMPTY;
  IF2_count = 0;
  IF1 = EMPTY;
  IF1_count = 1;
  inst_counter = 0;
  flush = FALSE;
  cycle = 1;
  inst_cycle = 1;
  
  IF1_stall  = NOSTALL;
  IF2_stall  = NOSTALL;
  ID_stall   = NOSTALL;
  EX_stall   = NOSTALL;
  MEM1_stall = NOSTALL;
  MEM2_stall = NOSTALL;
  MEM3_stall = NOSTALL;
  WB_stall   = NOSTALL;
  
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

  if( instcount == 0 ) return 1;

  return 0;
}

int check_stall( int reg )
{
  /* Check MEM1 */
  if( MEM1 != EMPTY )
    {
      /*  if( ( (*Instructions[ MEM1 ]).itype == DADD || (*Instructions[ MEM1 ]).itype == SUB ) && (*Instructions[ MEM1 ]).rd == reg ) return 1; */
      if( ( (*Instructions[ MEM1 ]).itype == LD ) && ( (*Instructions[ MEM1 ]).rt == reg  ) ) return 1;
    }
  /* Check MEM2 */
  if( MEM2 != EMPTY )
    {
      /* if( ( (*Instructions[ MEM2 ]).itype == DADD || (*Instructions[ MEM2 ]).itype == SUB ) && (*Instructions[ MEM2 ]).rd == reg ) return 1; */
      if( ( (*Instructions[ MEM2 ]).itype == LD ) && ( (*Instructions[ MEM2 ]).rt == reg  ) ) return 1;
    }

  return 0;
}

void if1()
{
  if( flush ) return;
  if( IF1_stall && (IF1 != EMPTY) ) return; /* stalled */
  if( inst_counter < instcount )
    {
      IF1 = inst_counter;
      IF1_count = inst_cycle;
    }
  else
    IF1 = EMPTY;
  if( IF1 != EMPTY )
    fprintf( fout, "I%d-IF1 ", IF1_count );
  inst_counter++;
}

void if2()
{
  if( IF2_stall && (IF2 != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", IF2_count );
      return;
    }
  if( IF2 != EMPTY )
    fprintf( fout, "I%d-IF2 ", IF2_count );
}

void id()
{
  if( ID_stall && (ID != EMPTY) )
    {
      fprintf( fout, "I%d-stall ", ID_count );
      return;
    }
  if( ID != EMPTY )
    fprintf( fout, "I%d-ID ", ID_count );
}

void ex()
{
  struct inst * curr;
  int i;

  if( EX == EMPTY )
    return;
  curr = Instructions[EX];
  if( EX_stall )
    {
      if( check_stall( (*curr).rs ) || check_stall( (*curr).rt ) ) /* check if stall still present */
	{
	  fprintf( fout, "I%d-stall ", EX_count );
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
  if( (*curr).itype == DADD )
    {
      if( check_stall( (*curr).rs ) || check_stall( (*curr).rt ) )
	{
	  IF1_stall = STALL;
	  IF2_stall = STALL;
	  ID_stall = STALL;
	  EX_stall = STALL;
	  fprintf( fout, "I%d-stall ", EX_count );
	  return;
	}
      if( (*curr).rt == NOTUSED )
	Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] + (*curr).value;
      else
	Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] + Registers[ RADDR((*curr).rt) ];
    }
  else if( (*curr).itype == SUB )
    {
      if( check_stall( (*curr).rs ) || check_stall( (*curr).rt ) )
	{
	  IF1_stall = STALL;
	  IF2_stall = STALL;
	  ID_stall = STALL;
	  EX_stall = STALL;
	  fprintf( fout, "I%d-stall ", EX_count );
	  return;
	}
      if( (*curr).rt == NOTUSED )
	Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] - (*curr).value;
      else
	Registers[ RADDR((*curr).rd) ] = Registers[ RADDR((*curr).rs) ] - Registers[ RADDR((*curr).rt) ];
    }
  else if( (*curr).itype == BNEZ )
    {
      /* check condition */
      if( check_stall( (*curr).rs ) )
	{
	  IF1_stall = STALL;
	  IF2_stall = STALL;
	  ID_stall = STALL;
	  EX_stall = STALL;
	  fprintf( fout, "I%d-stall ", EX_count );
	  return;
	}
      
      if( Registers[ RADDR((*curr).rs) ] != 0 )
	{
	  for( i = 0; i < instcount; ++i )
	    {
	      if( strcmp( (*Instructions[i]).label, (*curr).btarget ) == 0 )
		break;
	    }
	  if( i >= instcount )
	    {
	      printf("problem finding branch target!!\n");
	      return;
	    }
	  inst_counter = i;
	  /* flush pipeline */
	  flush = TRUE;
	}
      else
	{ /* branch was not taken, so it is done */
	  /* TODO: check if this is right, or if should let go through the pipeline */
	  EX = EMPTY;
	}
    }

  fprintf( fout, "I%d-EX ", EX_count );
}

void mem1()
{
  if( MEM1 == EMPTY )
    return;
  else if( MEM1_stall )
    {
      fprintf( fout, "I%d-stall ", MEM1_count );
      return;
    }
  else /* not stall, not empty */
    fprintf( fout, "I%d-MEM1 ", MEM1_count );
}

void mem2()
{
  struct inst * curr;
  if( MEM2 == EMPTY )
    return;
  else if( MEM2_stall )
    {
      fprintf( fout, "I%d-stall ", MEM2_count );
      return;
    }
  else /* not stall, not empty */
    {
      curr = Instructions[MEM2];
      /* TODO : currently not memory safe at all */
      if( (*curr).itype == LD )
	Registers[ RADDR((*curr).rt) ] = Memory[ MEMLOC( Registers[ RADDR((*curr).rs) ] + (*curr).value) ];
      else if( (*curr).itype == SD ) 
	Memory[ MEMLOC(Registers[ RADDR((*curr).rs) ] + (*curr).value) ] = Registers[ RADDR((*curr).rt) ];
      fprintf( fout, "I%d-MEM2 ", MEM2_count );
    }
}

void mem3()
{
  if( MEM3 == EMPTY )
    return;
  else if( MEM3_stall )
    {
      fprintf( fout, "I%d-stall ", MEM3_count );
      return;
    }
  else /* not stall, not empty */
    fprintf( fout, "I%d-MEM3 ", MEM3_count );
}

void wb()
{
  if( WB == EMPTY )
    return;
  else if( WB_stall )
    {
      fprintf( fout, "I%d-stall ", WB_count );
      return;
    }
  else /* not stall, not empty */
    fprintf( fout, "I%d-WB ", WB_count );
}
